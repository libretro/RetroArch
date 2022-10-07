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

#include <retro_assert.h>
#include <compat/strl.h>
#include <string/stdstring.h>
#include <file/config_file.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <streams/interface_stream.h>
#include <formats/rjson.h>
#include <lists/dir_list.h>
#include <file/archive_file.h>

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

#if defined(ANDROID)
#include "play_feature_delivery/play_feature_delivery.h"
#endif

/*************************/
/* Core Info Cache START */
/*************************/

#define CORE_INFO_CACHE_VERSION "1.2"
#define CORE_INFO_CACHE_DEFAULT_CAPACITY 8

/* TODO/FIXME: Apparently rzip compression is an issue on UWP */
#if defined(HAVE_ZLIB) && !(defined(__WINRT__) || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP)
#define CORE_INFO_CACHE_COMPRESS
#endif

typedef struct
{
   core_info_t *items;
   size_t length;
   size_t capacity;
   char *version;
   bool refresh;
} core_info_cache_list_t;

typedef struct
{
   core_info_t *core_info;
   core_info_cache_list_t *core_info_cache_list;
   char **current_string_val;
   struct string_list **current_string_list_val;
   uint32_t *current_entry_uint_val;
   bool *current_entry_bool_val;
   unsigned array_depth;
   unsigned object_depth;
   bool to_core_file_id;
   bool to_firmware;
} CCJSONContext;

/* Forward declarations */
static void core_info_free(core_info_t* info);
static uint32_t core_info_hash_string(const char *str);
#ifdef HAVE_CORE_INFO_CACHE
static core_info_cache_list_t *core_info_cache_list_new(void);
#endif
static void core_info_cache_add(core_info_cache_list_t *list,
      core_info_t *info, bool transfer);

static core_info_state_t core_info_st = {
#ifdef HAVE_COMPRESSION
   NULL,
#endif
   NULL,
   NULL,
   NULL
};

#ifdef HAVE_CORE_INFO_CACHE
/* JSON Handlers START */

static bool CCJSONObjectMemberHandler(void *context,
      const char *pValue, size_t length)
{
   CCJSONContext *pCtx = (CCJSONContext *)context;

   if ((pCtx->object_depth == 2) &&
       (pCtx->array_depth  == 1) &&
       length)
   {
      pCtx->current_string_val      = NULL;
      pCtx->current_string_list_val = NULL;
      pCtx->current_entry_uint_val  = NULL;
      pCtx->current_entry_bool_val  = NULL;
      pCtx->to_core_file_id         = false;
      pCtx->to_firmware             = false;

      switch (pValue[0])
      {
         case 'a':
            if (string_is_equal(pValue,      "authors"))
            {
               pCtx->current_string_val      = &pCtx->core_info->authors;
               pCtx->current_string_list_val = &pCtx->core_info->authors_list;
            }
            break;
         case 'c':
            if (string_is_equal(pValue,      "categories"))
            {
               pCtx->current_string_val      = &pCtx->core_info->categories;
               pCtx->current_string_list_val = &pCtx->core_info->categories_list;
            }
            else if (string_is_equal(pValue, "core_name"))
               pCtx->current_string_val      = &pCtx->core_info->core_name;
            else if (string_is_equal(pValue, "core_file_id"))
               pCtx->to_core_file_id         = true;
            break;
         case 'd':
            if (string_is_equal(pValue,      "display_name"))
               pCtx->current_string_val      = &pCtx->core_info->display_name;
            else if (string_is_equal(pValue, "display_version"))
               pCtx->current_string_val      = &pCtx->core_info->display_version;
            else if (string_is_equal(pValue, "databases"))
            {
               pCtx->current_string_val      = &pCtx->core_info->databases;
               pCtx->current_string_list_val = &pCtx->core_info->databases_list;
            }
            else if (string_is_equal(pValue, "description"))
               pCtx->current_string_val      = &pCtx->core_info->description;
            else if (string_is_equal(pValue, "database_match_archive_member"))
               pCtx->current_entry_bool_val  = &pCtx->core_info->database_match_archive_member;
            break;
         case 'f':
            if (string_is_equal(pValue,      "firmware"))
               pCtx->to_firmware             = true;
            break;
         case 'h':
            if (string_is_equal(pValue,      "has_info"))
               pCtx->current_entry_bool_val  = &pCtx->core_info->has_info;
            break;
         case 'l':
            if (string_is_equal(pValue,      "licenses"))
            {
               pCtx->current_string_val      = &pCtx->core_info->licenses;
               pCtx->current_string_list_val = &pCtx->core_info->licenses_list;
            }
            else if (string_is_equal(pValue, "is_experimental"))
               pCtx->current_entry_bool_val  = &pCtx->core_info->is_experimental;
            break;
         case 'n':
            if (string_is_equal(pValue,      "notes"))
            {
               pCtx->current_string_val      = &pCtx->core_info->notes;
               pCtx->current_string_list_val = &pCtx->core_info->note_list;
            }
            break;
         case 'p':
            if (string_is_equal(pValue,      "permissions"))
            {
               pCtx->current_string_val      = &pCtx->core_info->permissions;
               pCtx->current_string_list_val = &pCtx->core_info->permissions_list;
            }
            break;
         case 'r':
            if (string_is_equal(pValue,      "required_hw_api"))
            {
               pCtx->current_string_val      = &pCtx->core_info->required_hw_api;
               pCtx->current_string_list_val = &pCtx->core_info->required_hw_api_list;
            }
            break;
         case 's':
            if (string_is_equal(pValue,      "system_manufacturer"))
               pCtx->current_string_val      = &pCtx->core_info->system_manufacturer;
            else if (string_is_equal(pValue, "systemname"))
               pCtx->current_string_val      = &pCtx->core_info->systemname;
            else if (string_is_equal(pValue, "system_id"))
               pCtx->current_string_val      = &pCtx->core_info->system_id;
            else if (string_is_equal(pValue, "supported_extensions"))
            {
               pCtx->current_string_val      = &pCtx->core_info->supported_extensions;
               pCtx->current_string_list_val = &pCtx->core_info->supported_extensions_list;
            }
            else if (string_is_equal(pValue, "supports_no_game"))
               pCtx->current_entry_bool_val  = &pCtx->core_info->supports_no_game;
            else if (string_is_equal(pValue, "single_purpose"))
               pCtx->current_entry_bool_val  = &pCtx->core_info->single_purpose;
            else if (string_is_equal(pValue, "savestate_support_level"))
               pCtx->current_entry_uint_val  = &pCtx->core_info->savestate_support_level;
            break;
      }
   }
   else if ((pCtx->object_depth == 3) &&
            (pCtx->array_depth  == 1) &&
            length)
   {
      pCtx->current_string_val      = NULL;
      pCtx->current_entry_uint_val  = NULL;

      if (pCtx->to_core_file_id)
      {
         if (string_is_equal(pValue,      "str"))
            pCtx->current_string_val         = &pCtx->core_info->core_file_id.str;
         else if (string_is_equal(pValue, "hash"))
            pCtx->current_entry_uint_val     = &pCtx->core_info->core_file_id.hash;
      }
   }
   else if ((pCtx->object_depth == 3) &&
            (pCtx->array_depth  == 2) &&
            length)
   {
      pCtx->current_string_val      = NULL;
      pCtx->current_entry_bool_val  = NULL;

      if (pCtx->to_firmware && (pCtx->core_info->firmware_count > 0))
      {
         size_t firmware_idx = pCtx->core_info->firmware_count - 1;

         if (string_is_equal(pValue,      "path"))
            pCtx->current_string_val         = &pCtx->core_info->firmware[firmware_idx].path;
         else if (string_is_equal(pValue, "desc"))
            pCtx->current_string_val         = &pCtx->core_info->firmware[firmware_idx].desc;
         else if (string_is_equal(pValue, "optional"))
            pCtx->current_entry_bool_val     = &pCtx->core_info->firmware[firmware_idx].optional;
      }
   }
   else if ((pCtx->object_depth == 1) &&
            (pCtx->array_depth  == 0) &&
            length)
   {
      pCtx->current_string_val      = NULL;

      if (string_is_equal(pValue,         "version"))
         pCtx->current_string_val            = &pCtx->core_info_cache_list->version;
   }

   return true;
}

static bool CCJSONStringHandler(void *context,
      const char *pValue, size_t length)
{
   CCJSONContext *pCtx = (CCJSONContext*)context;

   if (     pCtx->current_string_val 
         && length 
         && !string_is_empty(pValue))
   {
      if (*pCtx->current_string_val)
         free(*pCtx->current_string_val);
      *pCtx->current_string_val = strdup(pValue);

      if (pCtx->current_string_list_val)
      {
         if (*pCtx->current_string_list_val)
            string_list_free(*pCtx->current_string_list_val);
         *pCtx->current_string_list_val = 
            string_split(*pCtx->current_string_val, "|");
      }
   }

   pCtx->current_string_val      = NULL;
   pCtx->current_string_list_val = NULL;

   return true;
}

static bool CCJSONNumberHandler(void *context,
      const char *pValue, size_t length)
{
   CCJSONContext *pCtx = (CCJSONContext*)context;

   if (pCtx->current_entry_uint_val)
      *pCtx->current_entry_uint_val = string_to_unsigned(pValue);

   pCtx->current_entry_uint_val = NULL;

   return true;
}

static bool CCJSONBoolHandler(void *context, bool value)
{
   CCJSONContext *pCtx = (CCJSONContext *)context;

   if (pCtx->current_entry_bool_val)
      *pCtx->current_entry_bool_val = value;

   pCtx->current_entry_bool_val     = NULL;

   return true;
}

static bool CCJSONStartObjectHandler(void *context)
{
   CCJSONContext *pCtx = (CCJSONContext*)context;

   pCtx->object_depth++;

   if ((pCtx->object_depth == 1) && (pCtx->array_depth == 0))
   {
      if (pCtx->core_info_cache_list)
         return false;

      pCtx->core_info_cache_list = core_info_cache_list_new();
      if (!pCtx->core_info_cache_list)
         return false;
   }
   else if ((pCtx->object_depth == 2) && (pCtx->array_depth == 1))
   {
      if (pCtx->core_info)
      {
         core_info_free(pCtx->core_info);
         free(pCtx->core_info);
         pCtx->core_info = NULL;
      }

      pCtx->core_info = (core_info_t*)calloc(1, sizeof(core_info_t));
      if (!pCtx->core_info)
         return false;

      /* Assume all cores have 'full' savestate support
       * by default */
      pCtx->core_info->savestate_support_level =
            CORE_INFO_SAVESTATE_DETERMINISTIC;
   }
   else if ((pCtx->object_depth == 3) && (pCtx->array_depth == 2))
   {
      if (pCtx->to_firmware)
      {
         size_t new_idx            = pCtx->core_info->firmware_count;
         core_info_firmware_t *tmp = (core_info_firmware_t*)
               realloc(pCtx->core_info->firmware,
                      (pCtx->core_info->firmware_count + 1) 
                     * sizeof(core_info_firmware_t));

         if (!tmp)
            return false;

         tmp[new_idx].path              = NULL;
         tmp[new_idx].desc              = NULL;
         tmp[new_idx].missing           = false;
         tmp[new_idx].optional          = false;

         pCtx->core_info->firmware      = tmp;
         pCtx->core_info->firmware_count++;
      }
   }

   return true;
}

static bool CCJSONEndObjectHandler(void *context)
{
   CCJSONContext *pCtx = (CCJSONContext*)context;

   if (     (pCtx->object_depth == 2) 
         && (pCtx->array_depth  == 1)
         && (pCtx->core_info))
   {
      core_info_cache_add(
            pCtx->core_info_cache_list, pCtx->core_info, true);
      free(pCtx->core_info);
      pCtx->core_info = NULL;
   }
   else if ((pCtx->object_depth == 3) && (pCtx->array_depth == 1))
      pCtx->to_core_file_id = false;

   retro_assert(pCtx->object_depth > 0);
   pCtx->object_depth--;

   return true;
}

static bool CCJSONStartArrayHandler(void *context)
{
   CCJSONContext *pCtx = (CCJSONContext*)context;
   pCtx->array_depth++;
   return true;
}

static bool CCJSONEndArrayHandler(void *context)
{
   CCJSONContext *pCtx = (CCJSONContext*)context;

   if ((pCtx->object_depth == 2) && (pCtx->array_depth == 2))
      pCtx->to_firmware = false;

   retro_assert(pCtx->array_depth > 0);
   pCtx->array_depth--;

   return true;
}

/* JSON Handlers END */
#endif

/* Note: 'dst' must be zero initialised, or memory
 * leaks will occur */
static void core_info_copy(core_info_t *src, core_info_t *dst)
{
   dst->path                      = src->path                 ? strdup(src->path)                 : NULL;
   dst->display_name              = src->display_name         ? strdup(src->display_name)         : NULL;
   dst->display_version           = src->display_version      ? strdup(src->display_version)      : NULL;
   dst->core_name                 = src->core_name            ? strdup(src->core_name)            : NULL;
   dst->system_manufacturer       = src->system_manufacturer  ? strdup(src->system_manufacturer)  : NULL;
   dst->systemname                = src->systemname           ? strdup(src->systemname)           : NULL;
   dst->system_id                 = src->system_id            ? strdup(src->system_id)            : NULL;
   dst->supported_extensions      = src->supported_extensions ? strdup(src->supported_extensions) : NULL;
   dst->authors                   = src->authors              ? strdup(src->authors)              : NULL;
   dst->permissions               = src->permissions          ? strdup(src->permissions)          : NULL;
   dst->licenses                  = src->licenses             ? strdup(src->licenses)             : NULL;
   dst->categories                = src->categories           ? strdup(src->categories)           : NULL;
   dst->databases                 = src->databases            ? strdup(src->databases)            : NULL;
   dst->notes                     = src->notes                ? strdup(src->notes)                : NULL;
   dst->required_hw_api           = src->required_hw_api      ? strdup(src->required_hw_api)      : NULL;
   dst->description               = src->description          ? strdup(src->description)          : NULL;

   dst->categories_list           = src->categories_list           ? string_list_clone(src->categories_list)           : NULL;
   dst->databases_list            = src->databases_list            ? string_list_clone(src->databases_list)            : NULL;
   dst->note_list                 = src->note_list                 ? string_list_clone(src->note_list)                 : NULL;
   dst->supported_extensions_list = src->supported_extensions_list ? string_list_clone(src->supported_extensions_list) : NULL;
   dst->authors_list              = src->authors_list              ? string_list_clone(src->authors_list)              : NULL;
   dst->permissions_list          = src->permissions_list          ? string_list_clone(src->permissions_list)          : NULL;
   dst->licenses_list             = src->licenses_list             ? string_list_clone(src->licenses_list)             : NULL;
   dst->required_hw_api_list      = src->required_hw_api_list      ? string_list_clone(src->required_hw_api_list)      : NULL;

   if (src->firmware_count > 0)
   {
      dst->firmware = (core_info_firmware_t*)calloc(src->firmware_count,
            sizeof(core_info_firmware_t));

      if (dst->firmware)
      {
         size_t i;

         dst->firmware_count = src->firmware_count;

         for (i = 0; i < src->firmware_count; i++)
         {
            dst->firmware[i].path     = src->firmware[i].path ? strdup(src->firmware[i].path) : NULL;
            dst->firmware[i].desc     = src->firmware[i].desc ? strdup(src->firmware[i].desc) : NULL;
            dst->firmware[i].missing  = src->firmware[i].missing;
            dst->firmware[i].optional = src->firmware[i].optional;
         }
      }
      else
         dst->firmware_count = 0;
   }

   dst->core_file_id.str              = src->core_file_id.str 
      ? strdup(src->core_file_id.str) : NULL;
   dst->core_file_id.hash             = src->core_file_id.hash;

   dst->savestate_support_level       = src->savestate_support_level;
   dst->has_info                      = src->has_info;
   dst->supports_no_game              = src->supports_no_game;
   dst->single_purpose                = src->single_purpose;
   dst->database_match_archive_member = src->database_match_archive_member;
   dst->is_experimental               = src->is_experimental;
   dst->is_locked                     = src->is_locked;
   dst->is_standalone_exempt          = src->is_standalone_exempt;
   dst->is_installed                  = src->is_installed;
}

/* Like core_info_copy, but transfers 'ownership'
 * of internal objects/data structures from 'src'
 * to 'dst' */
static void core_info_transfer(core_info_t *src, core_info_t *dst)
{
   dst->path                      = src->path;
   src->path                      = NULL;

   dst->display_name              = src->display_name;
   src->display_name              = NULL;

   dst->display_version           = src->display_version;
   src->display_version           = NULL;

   dst->core_name                 = src->core_name;
   src->core_name                 = NULL;

   dst->system_manufacturer       = src->system_manufacturer;
   src->system_manufacturer       = NULL;

   dst->systemname                = src->systemname;
   src->systemname                = NULL;

   dst->system_id                 = src->system_id;
   src->system_id                 = NULL;

   dst->supported_extensions      = src->supported_extensions;
   src->supported_extensions      = NULL;

   dst->authors                   = src->authors;
   src->authors                   = NULL;

   dst->permissions               = src->permissions;
   src->permissions               = NULL;

   dst->licenses                  = src->licenses;
   src->licenses                  = NULL;

   dst->categories                = src->categories;
   src->categories                = NULL;

   dst->databases                 = src->databases;
   src->databases                 = NULL;

   dst->notes                     = src->notes;
   src->notes                     = NULL;

   dst->required_hw_api           = src->required_hw_api;
   src->required_hw_api           = NULL;

   dst->description               = src->description;
   src->description               = NULL;

   dst->categories_list           = src->categories_list;
   src->categories_list           = NULL;

   dst->databases_list            = src->databases_list;
   src->databases_list            = NULL;

   dst->note_list                 = src->note_list;
   src->note_list                 = NULL;

   dst->supported_extensions_list = src->supported_extensions_list;
   src->supported_extensions_list = NULL;

   dst->authors_list              = src->authors_list;
   src->authors_list              = NULL;

   dst->permissions_list          = src->permissions_list;
   src->permissions_list          = NULL;

   dst->licenses_list             = src->licenses_list;
   src->licenses_list             = NULL;

   dst->required_hw_api_list      = src->required_hw_api_list;
   src->required_hw_api_list      = NULL;

   dst->firmware                  = src->firmware;
   dst->firmware_count            = src->firmware_count;
   src->firmware                  = NULL;
   src->firmware_count            = 0;

   dst->core_file_id.str              = src->core_file_id.str;
   src->core_file_id.str              = NULL;
   dst->core_file_id.hash             = src->core_file_id.hash;

   dst->savestate_support_level       = src->savestate_support_level;
   dst->has_info                      = src->has_info;
   dst->supports_no_game              = src->supports_no_game;
   dst->single_purpose                = src->single_purpose;
   dst->database_match_archive_member = src->database_match_archive_member;
   dst->is_experimental               = src->is_experimental;
   dst->is_locked                     = src->is_locked;
   dst->is_standalone_exempt          = src->is_standalone_exempt;
   dst->is_installed                  = src->is_installed;
}

static void core_info_cache_list_free(
      core_info_cache_list_t *core_info_cache_list)
{
   size_t i;

   if (!core_info_cache_list)
      return;

   for (i = 0; i < core_info_cache_list->length; i++)
   {
      core_info_t* info = (core_info_t*)&core_info_cache_list->items[i];
      core_info_free(info);
   }

   free(core_info_cache_list->items);

   if (core_info_cache_list->version)
      free(core_info_cache_list->version);

   free(core_info_cache_list);
}

static core_info_t *core_info_cache_find(
      core_info_cache_list_t *list, char *core_file_id)
{
   uint32_t hash;
   size_t i;

   if (!list ||
       string_is_empty(core_file_id))
      return NULL;

   hash = core_info_hash_string(core_file_id);

   for (i = 0; i < list->length; i++)
   {
      core_info_t *info = (core_info_t*)&list->items[i];

      if (!info)
         continue;

      if ((info->core_file_id.hash == hash) &&
          string_is_equal(info->core_file_id.str, core_file_id))
      {
         info->is_installed = true;
         return info;
      }
   }

   return NULL;
}

static void core_info_cache_add(
      core_info_cache_list_t *list, core_info_t *info,
      bool transfer)
{
   core_info_t *info_cache = NULL;

   if (   !list
       || !info
       || (info->core_file_id.hash == 0)
       || string_is_empty(info->core_file_id.str))
      return;

   if (list->length >= list->capacity)
   {
      size_t prev_capacity   = list->capacity;
      core_info_t *items_tmp = (core_info_t*)realloc(list->items,
            (list->capacity << 1) * sizeof(core_info_t));

      if (!items_tmp)
         return;

      list->capacity = list->capacity << 1;
      list->items    = items_tmp;

      memset(&list->items[prev_capacity], 0,
            (list->capacity - prev_capacity) * sizeof(core_info_t));
   }

   info_cache = (core_info_t*)&list->items[list->length];

   if (transfer)
      core_info_transfer(info, info_cache);
   else
      core_info_copy(info, info_cache);

   list->length++;
}

#ifdef HAVE_CORE_INFO_CACHE
static core_info_cache_list_t *core_info_cache_list_new(void)
{
   core_info_cache_list_t *core_info_cache_list = 
      (core_info_cache_list_t *)malloc(sizeof(*core_info_cache_list));
   if (!core_info_cache_list)
      return NULL;

   core_info_cache_list->length = 0;
   core_info_cache_list->items  = (core_info_t *)
      calloc(CORE_INFO_CACHE_DEFAULT_CAPACITY,
            sizeof(core_info_t));

   if (!core_info_cache_list->items)
   {
      core_info_cache_list_free(core_info_cache_list);
      return NULL;
   }

   core_info_cache_list->capacity = CORE_INFO_CACHE_DEFAULT_CAPACITY;
   core_info_cache_list->refresh  = false;
   core_info_cache_list->version  = NULL;

   return core_info_cache_list;
}

static core_info_cache_list_t *core_info_cache_read(const char *info_dir)
{
   intfstream_t *file                           = NULL;
   rjson_t *parser                              = NULL;
   CCJSONContext context                        = {0};
   core_info_cache_list_t *core_info_cache_list = NULL;
   char file_path[PATH_MAX_LENGTH];

   /* Check whether a 'force refresh' file
    * is present */
   if (string_is_empty(info_dir))
      strlcpy(file_path,
            FILE_PATH_CORE_INFO_CACHE_REFRESH, sizeof(file_path));
   else
      fill_pathname_join_special(file_path,
            info_dir, FILE_PATH_CORE_INFO_CACHE_REFRESH,
            sizeof(file_path));

   if (path_is_valid(file_path))
      return core_info_cache_list_new();

   /* Open info cache file */
   if (string_is_empty(info_dir))
      strlcpy(file_path, FILE_PATH_CORE_INFO_CACHE, sizeof(file_path));
   else
      fill_pathname_join_special(file_path, info_dir,
            FILE_PATH_CORE_INFO_CACHE,
            sizeof(file_path));

#if defined(HAVE_ZLIB)
   file = intfstream_open_rzip_file(file_path,
         RETRO_VFS_FILE_ACCESS_READ);
#else
   file = intfstream_open_file(file_path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
#endif

   if (!file)
      return core_info_cache_list_new();

   /* Parse info cache file */
   if (!(parser = rjson_open_stream(file)))
   {
      RARCH_ERR("[Core Info] Failed to create JSON parser\n");
      goto end;
   }

   rjson_set_options(parser,
           RJSON_OPTION_ALLOW_UTF8BOM
         | RJSON_OPTION_ALLOW_COMMENTS
         | RJSON_OPTION_ALLOW_UNESCAPED_CONTROL_CHARACTERS
         | RJSON_OPTION_REPLACE_INVALID_ENCODING);

   if (rjson_parse(parser, &context,
         CCJSONObjectMemberHandler,
         CCJSONStringHandler,
         CCJSONNumberHandler,
         CCJSONStartObjectHandler,
         CCJSONEndObjectHandler,
         CCJSONStartArrayHandler,
         CCJSONEndArrayHandler,
         CCJSONBoolHandler,
         NULL) /* Unused null handler */
         != RJSON_DONE)
   {
      RARCH_WARN("[Core Info] Error parsing chunk:\n---snip---\n%.*s\n---snip---\n",
            rjson_get_source_context_len(parser),
            rjson_get_source_context_buf(parser));
      RARCH_WARN("[Core Info] Error: Invalid JSON at line %d, column %d - %s.\n",
            (int)rjson_get_source_line(parser),
            (int)rjson_get_source_column(parser),
            (*rjson_get_error(parser) 
             ? rjson_get_error(parser) 
             : "format error"));

      /* Info cache is corrupt - discard it */
      core_info_cache_list_free(context.core_info_cache_list);
      core_info_cache_list = core_info_cache_list_new();
   }
   else
      core_info_cache_list = context.core_info_cache_list;

   rjson_free(parser);

   /* Clean up leftovers in the event of
    * a parsing error */
   if (context.core_info)
   {
      core_info_free(context.core_info);
      free(context.core_info);
   }

   if (!core_info_cache_list)
      goto end;

   /* If info cache file has the wrong version
    * number, discard it */
   if (string_is_empty(core_info_cache_list->version) ||
       !string_is_equal(core_info_cache_list->version,
            CORE_INFO_CACHE_VERSION))
   {
      RARCH_WARN("[Core Info] Core info cache has invalid version"
            " - forcing refresh (required v%s, found v%s)\n",
            CORE_INFO_CACHE_VERSION,
            core_info_cache_list->version);

      core_info_cache_list_free(context.core_info_cache_list);
      core_info_cache_list = core_info_cache_list_new();
   }

end:
   intfstream_close(file);
   free(file);

   return core_info_cache_list;
}
#endif

static bool core_info_cache_write(core_info_cache_list_t *list, const char *info_dir)
{
   intfstream_t *file    = NULL;
   rjsonwriter_t *writer = NULL;
   bool success          = false;
   char file_path[PATH_MAX_LENGTH];
   size_t i, j;

   if (!list)
      return false;

   /* Open info cache file */
   if (string_is_empty(info_dir))
      strlcpy(file_path, FILE_PATH_CORE_INFO_CACHE, sizeof(file_path));
   else
      fill_pathname_join_special(file_path, info_dir,
            FILE_PATH_CORE_INFO_CACHE,
            sizeof(file_path));

#if defined(CORE_INFO_CACHE_COMPRESS)
   file = intfstream_open_rzip_file(file_path,
         RETRO_VFS_FILE_ACCESS_WRITE);
#else
   file = intfstream_open_file(file_path,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
#endif

   if (!file)
   {
      RARCH_ERR("[Core Info] Failed to write to core info cache file: %s\n", file_path);
      return false;
   }

   /* Write info cache */
   if (!(writer = rjsonwriter_open_stream(file)))
   {
      RARCH_ERR("[Core Info] Failed to create JSON writer\n");
      goto end;
   }

#if defined(CORE_INFO_CACHE_COMPRESS)
   /* When compressing info cache, human readability
    * is not a factor - can skip all indentation
    * and new line characters */
   rjsonwriter_set_options(writer, RJSONWRITER_OPTION_SKIP_WHITESPACE);
#endif

   rjsonwriter_raw(writer, "{", 1);
   rjsonwriter_raw(writer, "\n", 1);
   rjsonwriter_add_spaces(writer, 2);
   rjsonwriter_add_string(writer, "version");
   rjsonwriter_raw(writer, ":", 1);
   rjsonwriter_raw(writer, " ", 1);
   rjsonwriter_add_string(writer, CORE_INFO_CACHE_VERSION);
   rjsonwriter_raw(writer, ",", 1);
   rjsonwriter_raw(writer, "\n", 1);
   rjsonwriter_add_spaces(writer, 2);
   rjsonwriter_add_string(writer, "items");
   rjsonwriter_raw(writer, ":", 1);
   rjsonwriter_raw(writer, " ", 1);
   rjsonwriter_raw(writer, "[", 1);
   rjsonwriter_raw(writer, "\n", 1);

   for (i = 0; i < list->length; i++)
   {
      core_info_t* info = &list->items[i];

      if (!info || !info->is_installed)
         continue;

      if (i > 0)
      {
         rjsonwriter_raw(writer, ",", 1);
         rjsonwriter_raw(writer, "\n", 1);
      }

      rjsonwriter_add_spaces(writer, 4);
      rjsonwriter_raw(writer, "{", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "display_name");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_add_string(writer, info->display_name);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "display_version");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_add_string(writer, info->display_version);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "core_name");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_add_string(writer, info->core_name);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "system_manufacturer");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_add_string(writer, info->system_manufacturer);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "systemname");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_add_string(writer, info->systemname);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "system_id");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_add_string(writer, info->system_id);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "supported_extensions");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_add_string(writer, info->supported_extensions);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "authors");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_add_string(writer, info->authors);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "permissions");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_add_string(writer, info->permissions);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "licenses");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_add_string(writer, info->licenses);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "categories");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_add_string(writer, info->categories);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "databases");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_add_string(writer, info->databases);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "notes");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_add_string(writer, info->notes);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "required_hw_api");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_add_string(writer, info->required_hw_api);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "description");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_add_string(writer, info->description);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      if (info->firmware_count > 0)
      {
         rjsonwriter_add_spaces(writer, 6);
         rjsonwriter_add_string(writer, "firmware");
         rjsonwriter_raw(writer, ":", 1);
         rjsonwriter_raw(writer, " ", 1);
         rjsonwriter_raw(writer, "[", 1);
         rjsonwriter_raw(writer, "\n", 1);

         for (j = 0; j < info->firmware_count; j++)
         {
            rjsonwriter_add_spaces(writer, 8);
            rjsonwriter_raw(writer, "{", 1);
            rjsonwriter_raw(writer, "\n", 1);
            rjsonwriter_add_spaces(writer, 10);
            rjsonwriter_add_string(writer, "path");
            rjsonwriter_raw(writer, ":", 1);
            rjsonwriter_raw(writer, " ", 1);
            rjsonwriter_add_string(writer, info->firmware[j].path);
            rjsonwriter_raw(writer, ",", 1);
            rjsonwriter_raw(writer, "\n", 1);
            rjsonwriter_add_spaces(writer, 10);
            rjsonwriter_add_string(writer, "desc");
            rjsonwriter_raw(writer, ":", 1);
            rjsonwriter_raw(writer, " ", 1);
            rjsonwriter_add_string(writer, info->firmware[j].desc);
            rjsonwriter_raw(writer, ",", 1);
            rjsonwriter_raw(writer, "\n", 1);
            rjsonwriter_add_spaces(writer, 10);
            rjsonwriter_add_string(writer, "optional");
            rjsonwriter_raw(writer, ":", 1);
            rjsonwriter_raw(writer, " ", 1);
            {
               bool value = info->firmware[j].optional;
               rjsonwriter_raw(writer, (value ? "true" : "false"), (value ? 4 : 5));
            }
            rjsonwriter_raw(writer, "\n", 1);
            rjsonwriter_add_spaces(writer, 8);
            rjsonwriter_raw(writer, "}", 1);

            if (j < info->firmware_count - 1)
               rjsonwriter_raw(writer, ",", 1);

            rjsonwriter_raw(writer, "\n", 1);
         }

         rjsonwriter_add_spaces(writer, 6);
         rjsonwriter_raw(writer, "]", 1);
         rjsonwriter_raw(writer, ",", 1);
         rjsonwriter_raw(writer, "\n", 1);
      }

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "core_file_id");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, "\n", 1);
      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_raw(writer, "{", 1);
      rjsonwriter_raw(writer, "\n", 1);
      rjsonwriter_add_spaces(writer, 8);
      rjsonwriter_add_string(writer, "str");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_add_string(writer, info->core_file_id.str);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);
      rjsonwriter_add_spaces(writer, 8);
      rjsonwriter_add_string(writer, "hash");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_rawf(writer, "%u", info->core_file_id.hash);
      rjsonwriter_raw(writer, "\n", 1);
      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_raw(writer, "}", 1);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "firmware_count");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_rawf(writer, "%u", info->firmware_count);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "savestate_support_level");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_rawf(writer, "%u", info->savestate_support_level);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "has_info");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      {
         bool value = info->has_info;
         rjsonwriter_raw(writer, (value ? "true" : "false"), (value ? 4 : 5));
      }
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "supports_no_game");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      {
         bool value = info->supports_no_game;
         rjsonwriter_raw(writer, (value ? "true" : "false"), (value ? 4 : 5));
      }
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "single_purpose");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      {
         bool value = info->single_purpose;
         rjsonwriter_raw(writer, (value ? "true" : "false"), (value ? 4 : 5));
      }
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "database_match_archive_member");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      {
         bool value = info->database_match_archive_member;
         rjsonwriter_raw(writer, (value ? "true" : "false"), (value ? 4 : 5));
      }
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "is_experimental");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      {
         bool value = info->is_experimental;
         rjsonwriter_raw(writer, (value ? "true" : "false"), (value ? 4 : 5));
      }
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 4);
      rjsonwriter_raw(writer, "}", 1);
   }

   rjsonwriter_raw(writer, "\n", 1);
   rjsonwriter_add_spaces(writer, 2);
   rjsonwriter_raw(writer, "]", 1);
   rjsonwriter_raw(writer, "\n", 1);
   rjsonwriter_raw(writer, "}", 1);
   rjsonwriter_raw(writer, "\n", 1);
   rjsonwriter_free(writer);

   RARCH_LOG("[Core Info] Wrote to cache file: %s\n", file_path);
   success = true;

   /* Remove 'force refresh' file, if required */
   if (string_is_empty(info_dir))
      strlcpy(file_path,
            FILE_PATH_CORE_INFO_CACHE_REFRESH, sizeof(file_path));
   else
      fill_pathname_join_special(file_path,
            info_dir, FILE_PATH_CORE_INFO_CACHE_REFRESH,
            sizeof(file_path));

   if (path_is_valid(file_path))
      filestream_delete(file_path);

end:
   intfstream_close(file);
   free(file);

   list->refresh = false;
   return success;
}

static void core_info_check_uninstalled(core_info_cache_list_t *list)
{
   size_t i;

   if (!list)
      return;

   for (i = 0; i < list->length; i++)
   {
      core_info_t *info = (core_info_t *)&list->items[i];

      if (!info)
         continue;

      if (!info->is_installed)
      {
         list->refresh = true;
         return;
      }
   }
}

/* When called, generates a temporary file
 * that will force an info cache refresh the
 * next time that core info is initialised with
 * caching enabled */
bool core_info_cache_force_refresh(const char *path_info)
{
   char file_path[PATH_MAX_LENGTH];

   /* Get 'force refresh' file path */
   if (string_is_empty(path_info))
      strlcpy(file_path,
            FILE_PATH_CORE_INFO_CACHE_REFRESH, sizeof(file_path));
   else
      fill_pathname_join_special(file_path,
            path_info, FILE_PATH_CORE_INFO_CACHE_REFRESH,
            sizeof(file_path));

   /* Generate a new, empty 'force refresh' file,
    * if required */
   if (!path_is_valid(file_path))
   {
      RFILE *refresh_file = filestream_open(
            file_path,
            RETRO_VFS_FILE_ACCESS_WRITE,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);

      if (!refresh_file)
         return false;

      /* We have to write something - just output
       * a single character */
      if (filestream_putc(refresh_file, 0) != 0)
      {
         filestream_close(refresh_file);
         return false;
      }

      filestream_close(refresh_file);
   }

   return true;
}

/***********************/
/* Core Info Cache END */
/***********************/

typedef struct
{
   const char *path;
   const char *filename;
} core_file_path_t;

typedef struct
{
   core_file_path_t *list;
   size_t size;
} core_file_path_list_t;

typedef struct
{
   const char *filename;
   uint32_t hash;
} core_aux_file_path_t;

typedef struct
{
   core_aux_file_path_t *list;
   size_t size;
} core_aux_file_path_list_t;

typedef struct
{
   struct string_list *dir_list;
   core_file_path_list_t *core_list;
   core_aux_file_path_list_t *lock_list;
   core_aux_file_path_list_t *standalone_exempt_list;
} core_path_list_t;

static uint32_t core_info_hash_string(const char *str)
{
   unsigned char c;
   uint32_t hash = (uint32_t)0x811c9dc5;
   while ((c = (unsigned char)*(str++)) != '\0')
      hash = ((hash * (uint32_t)0x01000193) ^ (uint32_t)c);
   return (hash ? hash : 1);
}

static void core_info_path_list_free(core_path_list_t *path_list)
{
   if (!path_list)
      return;

   if (path_list->core_list)
   {
      if (path_list->core_list->list)
         free(path_list->core_list->list);
      free(path_list->core_list);
   }

   if (path_list->lock_list)
   {
      if (path_list->lock_list->list)
         free(path_list->lock_list->list);
      free(path_list->lock_list);
   }

   if (path_list->standalone_exempt_list)
   {
      if (path_list->standalone_exempt_list->list)
         free(path_list->standalone_exempt_list->list);
      free(path_list->standalone_exempt_list);
   }

   if (path_list->dir_list)
      string_list_free(path_list->dir_list);

   free(path_list);
}

static core_path_list_t *core_info_path_list_new(const char *core_dir,
      const char *core_exts, bool show_hidden_files)
{
   core_path_list_t *path_list       = (core_path_list_t*)
         calloc(1, sizeof(*path_list));
   struct string_list *core_ext_list = NULL;
   bool dir_list_ok                  = false;
   char exts[32];
   size_t i;

   if (string_is_empty(core_exts) ||
       !path_list)
      goto error;

   if (!(core_ext_list = string_split(core_exts, "|")))
      goto error;

   /* Allocate list containers */
   path_list->dir_list               = string_list_new();
   path_list->core_list              = (core_file_path_list_t*)
         calloc(1, sizeof(*path_list->core_list));
   path_list->lock_list              = (core_aux_file_path_list_t*)
         calloc(1, sizeof(*path_list->lock_list));
   path_list->standalone_exempt_list = (core_aux_file_path_list_t*)
         calloc(1, sizeof(*path_list->standalone_exempt_list));

   if (   !path_list->dir_list
       || !path_list->core_list 
       || !path_list->lock_list
       || !path_list->standalone_exempt_list)
      goto error;

   /* Get list of file extensions to include
    * > core + lock */
   strlcpy(exts, core_exts, sizeof(exts));
#if defined(HAVE_DYNAMIC)
   /* > 'standalone exempt' */
   strlcat(exts, "|lck|lsae", sizeof(exts));
#else
   strlcat(exts, "|lck",      sizeof(exts));
#endif

   /* Fetch core directory listing */
   dir_list_ok = dir_list_append(path_list->dir_list,
         core_dir, exts, false, show_hidden_files,
               false, false);

#if defined(__WINRT__) || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
   {
      /* UWP: browse the optional packages for additional cores */
      struct string_list core_packages = {0};

      if (string_list_initialize(&core_packages))
      {
         uwp_fill_installed_core_packages(&core_packages);
         for (i = 0; i < core_packages.size; i++)
            dir_list_append(path_list->dir_list,
                  core_packages.elems[i].data, exts, false,
                        show_hidden_files, false, false);
         string_list_deinitialize(&core_packages);
      }
   }
#else
   /* Keep the old 'directory not found' behaviour */
   if (!dir_list_ok)
      goto error;
#endif

   /* Allocate sub lists */
   path_list->core_list->list              = (core_file_path_t*)
         malloc(path_list->dir_list->size *
               sizeof(*path_list->core_list->list));
   path_list->lock_list->list              = (core_aux_file_path_t*)
         malloc(path_list->dir_list->size *
               sizeof(*path_list->lock_list->list));
   path_list->standalone_exempt_list->list = (core_aux_file_path_t*)
         malloc(path_list->dir_list->size *
               sizeof(*path_list->standalone_exempt_list->list));

   if (!path_list->core_list->list ||
       !path_list->lock_list->list ||
       !path_list->standalone_exempt_list->list)
      goto error;

   /* Parse directory listing */
   for (i = 0; i < path_list->dir_list->size; i++)
   {
      const char *file_path = path_list->dir_list->elems[i].data;
      const char *filename  = NULL;
      const char *file_ext  = NULL;

      if (     string_is_empty(file_path)
          || !(filename = path_basename_nocompression(file_path))
          || !(file_ext = path_get_extension(filename)))
         continue;

      /* Check whether this is a core, lock or
       * 'standalone exempt' file */
      if (string_list_find_elem(core_ext_list, file_ext))
      {
         path_list->core_list->list[
               path_list->core_list->size].path     = file_path;
         path_list->core_list->list[
               path_list->core_list->size].filename = filename;
         path_list->core_list->size++;
      }
      else if (string_is_equal(file_ext, FILE_PATH_LOCK_EXTENSION_NO_DOT))
      {
         path_list->lock_list->list[
               path_list->lock_list->size].filename = filename;
         path_list->lock_list->list[
               path_list->lock_list->size].hash     = core_info_hash_string(filename);
         path_list->lock_list->size++;
      }
#if defined(HAVE_DYNAMIC)
      else if (string_is_equal(file_ext, FILE_PATH_STANDALONE_EXEMPT_EXTENSION_NO_DOT))
      {
         path_list->standalone_exempt_list->list[
               path_list->standalone_exempt_list->size].filename = filename;
         path_list->standalone_exempt_list->list[
               path_list->standalone_exempt_list->size].hash     = core_info_hash_string(filename);
         path_list->standalone_exempt_list->size++;
      }
#endif
   }

   string_list_free(core_ext_list);
   return path_list;

error:
   string_list_free(core_ext_list);
   core_info_path_list_free(path_list);
   return NULL;
}

static bool core_info_path_is_locked(
      core_aux_file_path_list_t *lock_list,
      const char *core_file_name)
{
   size_t i, len;
   uint32_t hash;
   char lock_filename[NAME_MAX_LENGTH];

   if (lock_list->size < 1)
      return false;

   len                  = strlcpy(lock_filename, core_file_name,
         sizeof(lock_filename));
   lock_filename[len  ] = '.';
   lock_filename[len+1] = 'l';
   lock_filename[len+2] = 'c';
   lock_filename[len+3] = 'k';
   lock_filename[len+4] = '\0';

   hash = core_info_hash_string(lock_filename);

   for (i = 0; i < lock_list->size; i++)
   {
      core_aux_file_path_t *lock_file = &lock_list->list[i];

      if ((lock_file->hash == hash) &&
          string_is_equal(lock_file->filename, lock_filename))
         return true;
   }

   return false;
}

static bool core_info_path_is_standalone_exempt(
      core_aux_file_path_list_t *exempt_list,
      const char *core_file_name)
{
   size_t i, len;
   uint32_t hash;
   char exempt_filename[NAME_MAX_LENGTH];

   if (exempt_list->size < 1)
      return false;

   len                    = strlcpy(exempt_filename, core_file_name,
         sizeof(exempt_filename));
   exempt_filename[len  ] = '.';
   exempt_filename[len+1] = 'l';
   exempt_filename[len+2] = 's';
   exempt_filename[len+3] = 'a';
   exempt_filename[len+4] = 'e';
   exempt_filename[len+5] = '\0';

   hash = core_info_hash_string(exempt_filename);

   for (i = 0; i < exempt_list->size; i++)
   {
      core_aux_file_path_t *exempt_file = &exempt_list->list[i];

      if ((exempt_file->hash == hash) &&
          string_is_equal(exempt_file->filename, exempt_filename))
         return true;
   }

   return false;
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
   size_t i;
   uint32_t hash;
   char core_file_id[256];

   if (!list ||
       string_is_empty(core_path) ||
       !core_info_get_file_id(path_basename_nocompression(core_path),
            core_file_id, sizeof(core_file_id)))
      return NULL;

   hash = core_info_hash_string(core_file_id);

   for (i = 0; i < list->count; i++)
   {
      core_info_t *info = &list->list[i];

      if ((info->core_file_id.hash == hash) &&
          string_is_equal(info->core_file_id.str, core_file_id))
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

   firmware = (core_info_firmware_t*)calloc(
         firmware_count, sizeof(*firmware));

   if (!firmware)
      return;

   for (i = 0; i < firmware_count; i++)
   {
      char prefix[12];
      char path_key[64];
      char desc_key[64];
      char opt_key[64];
      struct config_entry_list *entry = NULL;
      bool tmp_bool                   = false;

      prefix[0]   = '\0';

      snprintf(prefix,   sizeof(prefix),   "firmware%u_", i);
      strlcpy(path_key,  prefix,           sizeof(path_key));
      strlcat(path_key,  "path",           sizeof(path_key));
      strlcpy(desc_key,  prefix,           sizeof(desc_key));
      strlcat(desc_key,  "desc",           sizeof(desc_key));
      strlcpy(opt_key,   prefix,           sizeof(opt_key));
      strlcat(opt_key,   "opt",            sizeof(opt_key));

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
      const char *core_file_id, const char *info_dir)
{
   if (!string_is_empty(info_dir))
   {
      char info_path[PATH_MAX_LENGTH];
      fill_pathname_join_special(info_path, info_dir,
            core_file_id, sizeof(info_path));
      return config_file_new_from_path_to_string(info_path);
   }
   return config_file_new_from_path_to_string(core_file_id);
}

static void core_info_parse_config_file(
      core_info_list_t *list, core_info_t *info,
      config_file_t *conf)
{
   bool tmp_bool                   = false;
   struct config_entry_list *entry = config_get_entry(conf, "display_name");

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

   if (config_get_bool(conf, "single_purpose",
            &tmp_bool))
      info->single_purpose = tmp_bool;

   if (config_get_bool(conf, "database_match_archive_member",
            &tmp_bool))
      info->database_match_archive_member = tmp_bool;

   if (config_get_bool(conf, "is_experimental",
            &tmp_bool))
      info->is_experimental = tmp_bool;


   /* Savestate support level is slightly more complex,
    * since it is a value derived from two configuration
    * parameters */

   /* > Assume all cores have 'full' savestate support
    *   by default */
   info->savestate_support_level =
         CORE_INFO_SAVESTATE_DETERMINISTIC;

   /* > Check whether savestate functionality is defined
    *   in the info file */
   if (config_get_bool(conf, "savestate", &tmp_bool))
   {
      if (tmp_bool)
      {
         /* Check if savestate features are defined */
         entry = config_get_entry(conf, "savestate_features");

         if (entry && !string_is_empty(entry->value))
         {
            if (string_is_equal(entry->value, "basic"))
               info->savestate_support_level =
                     CORE_INFO_SAVESTATE_BASIC;
            else if (string_is_equal(entry->value, "serialized"))
               info->savestate_support_level =
                     CORE_INFO_SAVESTATE_SERIALIZED;
         }
      }
      else
         info->savestate_support_level =
               CORE_INFO_SAVESTATE_DISABLED;
   }

   core_info_resolve_firmware(info, conf);

   info->has_info = true;
   list->info_count++;
}

static void core_info_list_resolve_all_extensions(
      core_info_list_t *core_info_list)
{
   size_t _len           = 0;
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

   if (!(all_ext      = (char*)calloc(1, all_ext_len)))
      return;

   core_info_list->all_ext = all_ext;

   for (i = 0; i < core_info_list->count; i++)
   {
      if (!core_info_list->list[i].supported_extensions)
         continue;

             strlcat(core_info_list->all_ext,
            core_info_list->list[i].supported_extensions, all_ext_len);
      _len = strlcat(core_info_list->all_ext, "|", all_ext_len);
   }
#ifdef HAVE_7ZIP
   core_info_list->all_ext[_len  ] = '7';
   core_info_list->all_ext[_len+1] = 'z';
   core_info_list->all_ext[_len+2] = '|';
   core_info_list->all_ext[_len+3] = '\0';
   _len                           += 3;
#endif
#ifdef HAVE_ZLIB
   core_info_list->all_ext[_len  ] = 'z';
   core_info_list->all_ext[_len+1] = 'i';
   core_info_list->all_ext[_len+2] = 'p';
   core_info_list->all_ext[_len+3] = '|';
   core_info_list->all_ext[_len+4] = '\0';
#endif
}

static void core_info_free(core_info_t* info)
{
   size_t i;

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

   for (i = 0; i < info->firmware_count; i++)
   {
      if (info->firmware[i].path)
         free(info->firmware[i].path);
      if (info->firmware[i].desc)
         free(info->firmware[i].desc);
      info->firmware[i].path = NULL;
      info->firmware[i].desc = NULL;
   }
   free(info->firmware);

   free(info->core_file_id.str);
}

static void core_info_list_free(core_info_list_t *core_info_list)
{
   size_t i;

   if (!core_info_list)
      return;

   for (i = 0; i < core_info_list->count; i++)
   {
      core_info_t *info = (core_info_t*)&core_info_list->list[i];
      core_info_free(info);
   }

   free(core_info_list->all_ext);
   free(core_info_list->list);
   free(core_info_list);
}

static core_info_list_t *core_info_list_new(const char *path,
      const char *libretro_info_dir,
      const char *exts,
      bool dir_show_hidden_files,
      bool enable_cache,
      bool *cache_supported)
{
   size_t i;
   core_info_t *core_info                       = NULL;
   core_info_list_t *core_info_list             = NULL;
   core_info_cache_list_t *core_info_cache_list = NULL;
   const char *info_dir                         = libretro_info_dir;
   core_path_list_t *path_list                  = core_info_path_list_new(
         path, exts, dir_show_hidden_files);
   if (!path_list)
      goto error;

   if (!(core_info_list = (core_info_list_t*)malloc(sizeof(*core_info_list))))
      goto error;

   core_info_list->list       = NULL;
   core_info_list->count      = 0;
   core_info_list->info_count = 0;
   core_info_list->all_ext    = NULL;

   if (!(core_info = (core_info_t*)calloc(path_list->core_list->size,
         sizeof(*core_info))))
   {
      core_info_list_free(core_info_list);
      goto error;
   }

   core_info_list->list  = core_info;
   core_info_list->count = path_list->core_list->size;

#ifdef HAVE_CORE_INFO_CACHE
   /* Read core info cache, if enabled */
   if (enable_cache)
   {
      core_info_cache_list = core_info_cache_read(info_dir);
      if (!core_info_cache_list)
         goto error;
   }
#endif

   for (i = 0; i < path_list->core_list->size; i++)
   {
      core_info_t *info           = &core_info[i];
      core_file_path_t *core_file = &path_list->core_list->list[i];
      const char *base_path       = core_file->path;
      const char *core_filename   = core_file->filename;
      config_file_t *conf         = NULL;
      char core_file_id[256];

      if (!core_info_get_file_id(core_filename, core_file_id,
               sizeof(core_file_id)))
         continue;

      /* If info cache is available, search for
       * current core */
      if (core_info_cache_list)
      {
         core_info_t *info_cache = core_info_cache_find(
               core_info_cache_list, core_file_id);

         if (info_cache)
         {
            core_info_copy(info_cache, info);

            /* Core path is 'dynamic', and cannot
             * be cached (i.e. core directory may
             * change between runs) */
            if (info->path)
               free(info->path);
            info->path = strdup(base_path);

            /* Core lock status is 'dynamic', and
             * cannot be cached */
            info->is_locked = core_info_path_is_locked(
                  path_list->lock_list, core_filename);

            /* Core 'standalone exempt' status is 'dynamic',
             * and cannot be cached
             * > It is also dependent upon whether the core
             *   supports contentless operation */
            info->is_standalone_exempt = info->supports_no_game &&
                  core_info_path_is_standalone_exempt(
                        path_list->standalone_exempt_list,
                        core_filename);

            /* 'info_count' is normally incremented inside
             * core_info_parse_config_file(). If core entry
             * is cached, must instead increment the value
             * here */
            if (info->has_info)
               core_info_list->info_count++;

            continue;
         }
      }

      /* Cache core path */
      info->path              = strdup(base_path);

      /* Get core lock status */
      info->is_locked         = core_info_path_is_locked(
            path_list->lock_list, core_filename);

      /* Cache core file 'id' */
      info->core_file_id.str  = strdup(core_file_id);
      info->core_file_id.hash = core_info_hash_string(core_file_id);

      strlcat(core_file_id, ".info", sizeof(core_file_id));

      /* Parse core info file */
      if ((conf = core_info_get_config_file(core_file_id, info_dir)))
      {
         core_info_parse_config_file(core_info_list, info, conf);
         config_file_free(conf);
      }

      /* Get fallback display name, if required */
      if (!info->display_name)
         info->display_name = strdup(core_filename);

      /* Get core 'standalone exempt' status */
      info->is_standalone_exempt = info->supports_no_game &&
            core_info_path_is_standalone_exempt(
                  path_list->standalone_exempt_list,
                  core_filename);

      info->is_installed = true;

      /* If info cache is enabled and we reach this
       * point, current core is uncached
       * > Add it to the list, and trigger a cache
       *   refresh */
      if (core_info_cache_list)
      {
         core_info_cache_add(core_info_cache_list, info, false);
         core_info_cache_list->refresh = true;
      }
   }

   core_info_list_resolve_all_extensions(core_info_list);

   /* If info cache is enabled
    * > Check whether any cached cores have been
    *   uninstalled since the last run (triggers
    *   a refresh)
    * > Write new cache to disk if updates are
    *   required */
   *cache_supported = true;
   if (core_info_cache_list)
   {
      core_info_check_uninstalled(core_info_cache_list);

      if (core_info_cache_list->refresh)
         *cache_supported = core_info_cache_write(
               core_info_cache_list, info_dir);

      core_info_cache_list_free(core_info_cache_list);
   }

   core_info_path_list_free(path_list);
   return core_info_list;

error:
   core_info_path_list_free(path_list);
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
   core_info_state_t *p_coreinfo = &core_info_st;
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

   if (!(info = core_info_find_internal(
         core_info_list, core_path)))
      return false;

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

void core_info_free_current_core(void)
{
   core_info_state_t *p_coreinfo = &core_info_st;
   if (p_coreinfo->current)
      free(p_coreinfo->current);
   p_coreinfo->current = NULL;
}

bool core_info_init_current_core(void)
{
   core_info_state_t *p_coreinfo          = &core_info_st;
   core_info_t *current                   = (core_info_t*)
      malloc(sizeof(*current));
   if (!current)
      return false;
   current->has_info                      = false;
   current->supports_no_game              = false;
   current->single_purpose                = false;
   current->database_match_archive_member = false;
   current->is_experimental               = false;
   current->is_locked                     = false;
   current->is_standalone_exempt          = false;
   current->is_installed                  = false;
   current->firmware_count                = 0;
   current->savestate_support_level       = CORE_INFO_SAVESTATE_DETERMINISTIC;
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
   core_info_state_t *p_coreinfo          = &core_info_st;
   if (!core)
      return false;
   *core = p_coreinfo->current;
   return true;
}

void core_info_deinit_list(void)
{
   core_info_state_t *p_coreinfo          = &core_info_st;
   if (p_coreinfo->curr_list)
      core_info_list_free(p_coreinfo->curr_list);
   p_coreinfo->curr_list = NULL;
}

bool core_info_init_list(
      const char *path_info, const char *dir_cores,
      const char *exts,  bool dir_show_hidden_files,
      bool enable_cache, bool *cache_supported)
{
   core_info_state_t *p_coreinfo          = &core_info_st;
   if (!(p_coreinfo->curr_list            = core_info_list_new(
               dir_cores,
               !string_is_empty(path_info) 
               ? path_info 
               : dir_cores,
               exts,
               dir_show_hidden_files,
               enable_cache,
               cache_supported)))
      return false;
   return true;
}

bool core_info_get_list(core_info_list_t **core)
{
   core_info_state_t *p_coreinfo          = &core_info_st;
   if (!core)
      return false;
   *core = p_coreinfo->curr_list;
   return true;
}

/* Returns number of installed cores */
size_t core_info_count(void)
{
   core_info_state_t *p_coreinfo          = &core_info_st;
   if (p_coreinfo && p_coreinfo->curr_list)
      return p_coreinfo->curr_list->count;
   return 0;
}

bool core_info_list_update_missing_firmware(
      core_info_ctx_firmware_t *info, bool *set_missing_bios)
{
   core_info_state_t *p_coreinfo          = &core_info_st;
   if (info)
      return core_info_list_update_missing_firmware_internal(
            p_coreinfo->curr_list,
            info->path, info->directory.system,
            set_missing_bios);
   return false;
}

bool core_info_load(const char *core_path)
{
   core_info_state_t *p_coreinfo = &core_info_st;
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
   core_info_state_t *p_coreinfo = &core_info_st;
   core_info_t *info             = NULL;

   if (!core_info || !p_coreinfo->curr_list)
      return false;

   if (!(info = core_info_find_internal(p_coreinfo->curr_list, core_path)))
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
   core_info_state_t *p_coreinfo = &core_info_st;

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

   if (   string_is_empty(core_path_a)
       || string_is_empty(core_path_b)
       || !core_info_get_file_id(
          path_basename_nocompression(core_path_a),
            core_file_id_a, sizeof(core_file_id_a))
       || !core_info_get_file_id(
          path_basename_nocompression(core_path_b),
            core_file_id_b, sizeof(core_file_id_b)))
      return false;

   return string_is_equal(core_file_id_a, core_file_id_b);
}

bool core_info_database_match_archive_member(const char *database_path)
{
   char      *database           = NULL;
   const char      *new_path     = path_basename_nocompression(
         database_path);
   core_info_state_t *p_coreinfo = NULL;

   if (string_is_empty(new_path))
      return false;
   if (!(database = strdup(new_path)))
      return false;

   path_remove_extension(database);

   p_coreinfo                     = &core_info_st;

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

   p_coreinfo                    = &core_info_st;

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

bool core_info_list_get_display_name(
      core_info_list_t *core_info_list,
      const char *core_path, char *s, size_t len)
{
   core_info_t *info;

   if (!core_info_list)
     return false;

   info = core_info_find_internal(
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
core_updater_info_t *core_info_get_core_updater_info(
      const char *info_path)
{
   struct config_entry_list 
      *entry                 = NULL;
   bool tmp_bool             = false;
   core_updater_info_t *info = NULL;
   config_file_t *conf       = NULL;

   if (string_is_empty(info_path))
      return NULL;

   /* Read config file */
   if (!(conf = config_file_new_from_path_to_string(info_path)))
      return NULL;

   /* Create info struct */
   if (!(info = (core_updater_info_t*)malloc(sizeof(*info))))
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
   if (!a || !b || string_is_empty(a->path) || string_is_empty(b->path))
      return 0;
   return strcasecmp(a->path, b->path);
}

static int core_info_qsort_func_display_name(const core_info_t *a,
      const core_info_t *b)
{
   if (     !a
         || !b
         || string_is_empty(a->display_name) 
         || string_is_empty(b->display_name))
      return 0;
   return strcasecmp(a->display_name, b->display_name);
}

static int core_info_qsort_func_core_name(const core_info_t *a,
      const core_info_t *b)
{
   if (     !a
         || !b
         || string_is_empty(a->core_name) 
         || string_is_empty(b->core_name))
      return 0;
   return strcasecmp(a->core_name, b->core_name);
}

static int core_info_qsort_func_system_name(const core_info_t *a,
      const core_info_t *b)
{
   if (
            !a
         || !b
         || string_is_empty(a->systemname) 
         || string_is_empty(b->systemname))
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

bool core_info_current_supports_savestate(void)
{
   core_info_state_t *p_coreinfo = &core_info_st;

   /* If no core is currently loaded, assume
    * by default that all savestate functionality
    * is supported */
   if (!p_coreinfo->current)
      return true;

   return p_coreinfo->current->savestate_support_level >=
         CORE_INFO_SAVESTATE_BASIC;
}

bool core_info_current_supports_rewind(void)
{
   core_info_state_t *p_coreinfo = &core_info_st;

   /* If no core is currently loaded, assume
    * by default that all savestate functionality
    * is supported */
   if (!p_coreinfo->current)
      return true;

   return p_coreinfo->current->savestate_support_level >=
         CORE_INFO_SAVESTATE_SERIALIZED;
}

bool core_info_current_supports_netplay(void)
{
   core_info_state_t *p_coreinfo = &core_info_st;

   /* If no core is currently loaded, assume
    * by default that all savestate functionality
    * is supported */
   if (!p_coreinfo->current)
      return true;

   return p_coreinfo->current->savestate_support_level >=
         CORE_INFO_SAVESTATE_DETERMINISTIC;
}

bool core_info_current_supports_runahead(void)
{
   core_info_state_t *p_coreinfo = &core_info_st;

   /* If no core is currently loaded, assume
    * by default that all savestate functionality
    * is supported */
   if (!p_coreinfo->current)
      return true;

   return p_coreinfo->current->savestate_support_level >=
         CORE_INFO_SAVESTATE_DETERMINISTIC;
}

static bool core_info_update_core_aux_file(const char *path, bool create)
{
   bool aux_file_exists = false;

   if (string_is_empty(path))
      return false;

   /* Check whether aux file exists */
   aux_file_exists = path_is_valid(path);

   /* Create or delete aux file, as required */
   if (create && !aux_file_exists)
   {
      RFILE *aux_file = filestream_open(path,
            RETRO_VFS_FILE_ACCESS_WRITE,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);

      if (!aux_file)
         return false;

      /* We have to write something - just output
       * a single character */
      if (filestream_putc(aux_file, 0) != 0)
      {
         filestream_close(aux_file);
         return false;
      }

      filestream_close(aux_file);
   }
   else if (!create && aux_file_exists)
      if (filestream_delete(path) != 0)
         return false;

   return true;
}

/* Sets 'locked' status of specified core
 * > Returns true if successful
 * > Like all functions that access the cached
 *   core info list this is *not* thread safe */
bool core_info_set_core_lock(const char *core_path, bool lock)
{
   size_t len;
   core_info_t *core_info = NULL;
   char lock_file_path[PATH_MAX_LENGTH];

#if defined(ANDROID)
   /* Play Store builds do not support
    * core locking */
   if (play_feature_delivery_enabled())
      return false;
#endif

   /* Search for specified core */
   if (
           string_is_empty(core_path)
       || !core_info_find(core_path, &core_info)
       || string_is_empty(core_info->path))
      return false;

   /* Get lock file path */
   len                   = strlcpy(
         lock_file_path, core_info->path, sizeof(lock_file_path));
   lock_file_path[len  ] = '.';
   lock_file_path[len+1] = 'l';
   lock_file_path[len+2] = 'c';
   lock_file_path[len+3] = 'k';
   lock_file_path[len+4] = '\0';

   /* Create or delete lock file, as required */
   if (!core_info_update_core_aux_file(lock_file_path, lock))
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
   size_t len;
   core_info_t *core_info     = NULL;
   const char *core_file_path = NULL;
   bool is_locked             = false;
   char lock_file_path[PATH_MAX_LENGTH];

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
   len                   = strlcpy(
         lock_file_path, core_file_path,
         sizeof(lock_file_path));
   lock_file_path[len  ] = '.';
   lock_file_path[len+1] = 'l';
   lock_file_path[len+2] = 'c';
   lock_file_path[len+3] = 'k';
   lock_file_path[len+4] = '\0';

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

/* Sets 'standalone exempt' status of specified core
 * > A 'standalone exempt' core will not be shown
 *   in the contentless cores menu when display type
 *   is set to 'custom'
 * > Returns true if successful
 * > Returns false if core does not support
 *   contentless operation
 * > *Not* thread safe */
bool core_info_set_core_standalone_exempt(const char *core_path, bool exempt)
{
#if defined(HAVE_DYNAMIC)
   size_t _len;
   core_info_t *core_info = NULL;
   char exempt_file_path[PATH_MAX_LENGTH];

   /* Search for specified core */
   if (    string_is_empty(core_path)
       || !core_info_find(core_path, &core_info)
       || string_is_empty(core_info->path)
       || !core_info->supports_no_game)
      return false;

   /* Get 'standalone exempt' file path */
   _len = strlcpy(exempt_file_path, core_info->path,
         sizeof(exempt_file_path));
   exempt_file_path[_len  ] = '.';
   exempt_file_path[_len+1] = 'l';
   exempt_file_path[_len+2] = 's';
   exempt_file_path[_len+3] = 'a';
   exempt_file_path[_len+4] = 'e';
   exempt_file_path[_len+5] = '\0';

   /* Create or delete 'standalone exempt' file, as required */
   if (!core_info_update_core_aux_file(exempt_file_path, exempt))
      return false;

   /* File operations were successful - update
    * core info entry */
   core_info->is_standalone_exempt = exempt;

   return true;
#else
   /* Static platforms do not support the contentless
    * cores menu */
   return false;
#endif
}

/* Fetches 'standalone exempt' status of specified core
 * > Returns true if core should be excluded from
 *   the contentless cores menu when display type is
 *   set to 'custom'
 * > *Not* thread safe */
bool core_info_get_core_standalone_exempt(const char *core_path)
{
#if defined(HAVE_DYNAMIC)
   size_t _len;
   core_info_t *core_info = NULL;
   bool is_exempt         = false;
   char exempt_file_path[PATH_MAX_LENGTH];

   /* Search for specified core */
   if (    string_is_empty(core_path)
       || !core_info_find(core_path, &core_info)
       ||  string_is_empty(core_info->path)
       || !core_info->supports_no_game)
      return false;

   /* Get 'standalone exempt' file path */
   _len                     = strlcpy(
         exempt_file_path, core_info->path,
         sizeof(exempt_file_path));
   exempt_file_path[_len  ] = '.';
   exempt_file_path[_len+1] = 'l';
   exempt_file_path[_len+2] = 's';
   exempt_file_path[_len+3] = 'a';
   exempt_file_path[_len+4] = 'e';
   exempt_file_path[_len+5] = '\0';

   /* Check whether 'standalone exempt' file exists */
   is_exempt = path_is_valid(exempt_file_path);

   /* Ensure that core info 'is_standalone_exempt'
    * field is up to date */
   core_info->is_standalone_exempt = is_exempt;

   return is_exempt;
#else
   /* Static platforms do not support the contentless
    * cores menu */
   return false;
#endif
}
