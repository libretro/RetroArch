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

#ifndef CORE_INFO_H_
#define CORE_INFO_H_

#include <stddef.h>

#include <lists/string_list.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

enum core_info_list_qsort_type
{
   CORE_INFO_LIST_SORT_PATH = 0,
   CORE_INFO_LIST_SORT_DISPLAY_NAME,
   CORE_INFO_LIST_SORT_CORE_NAME,
   CORE_INFO_LIST_SORT_SYSTEM_NAME
};

typedef struct
{
   char *path;
   char *desc;
   /* Set missing once to avoid opening
    * the same file several times. */
   bool missing;
   bool optional;
} core_info_firmware_t;

/* Simple container/convenience struct for
 * holding the 'id' of a core file
 * > 'str' is the filename without extension or
 *   platform-specific suffix
 * > 'hash' is a hash key used for efficient core
 *   list searches */
typedef struct
{
   char *str;
   uint32_t hash;
} core_file_id_t;

typedef struct
{
   char *path;
   char *display_name;
   char *display_version;
   char *core_name;
   char *system_manufacturer;
   char *systemname;
   char *system_id;
   char *supported_extensions;
   char *authors;
   char *permissions;
   char *licenses;
   char *categories;
   char *databases;
   char *notes;
   char *required_hw_api;
   char *description;
   struct string_list *categories_list;
   struct string_list *databases_list;
   struct string_list *note_list;
   struct string_list *supported_extensions_list;
   struct string_list *authors_list;
   struct string_list *permissions_list;
   struct string_list *licenses_list;
   struct string_list *required_hw_api_list;
   core_info_firmware_t *firmware;
   core_file_id_t core_file_id; /* ptr alignment */
   size_t firmware_count;
   bool has_info;
   bool supports_no_game;
   bool database_match_archive_member;
   bool is_experimental;
   bool is_locked;
   bool is_installed;
} core_info_t;

/* A subset of core_info parameters required for
 * core updater tasks */
typedef struct
{
   char *display_name;
   char *description;
   char *licenses;
   bool is_experimental;
} core_updater_info_t;

typedef struct
{
   core_info_t *list;
   char *all_ext;
   size_t count;
   size_t info_count;
} core_info_list_t;

typedef struct core_info_ctx_firmware
{
   const char *path;
   struct
   {
      const char *system;
   } directory;
} core_info_ctx_firmware_t;

struct core_info_state
{
#ifdef HAVE_COMPRESSION
   const struct string_list *tmp_list;
#endif
   const char *tmp_path;
   core_info_t *current;
   core_info_list_t *curr_list;
};

typedef struct core_info_state core_info_state_t;

/* Non-reentrant, does not allocate. Returns pointer to internal state. */
void core_info_list_get_supported_cores(core_info_list_t *list,
      const char *path, const core_info_t **infos, size_t *num_infos);

bool core_info_list_get_display_name(core_info_list_t *list,
      const char *core_path, char *s, size_t len);

/* Returns core_info parameters required for
 * core updater tasks, read from specified file.
 * Returned core_updater_info_t object must be
 * freed using core_info_free_core_updater_info().
 * Returns NULL if 'path' is invalid. */
core_updater_info_t *core_info_get_core_updater_info(const char *info_path);
void core_info_free_core_updater_info(core_updater_info_t *info);

core_info_t *core_info_get(core_info_list_t *list, size_t i);

void core_info_free_current_core(void);

bool core_info_init_current_core(void);

bool core_info_get_current_core(core_info_t **core);

void core_info_deinit_list(void);

bool core_info_init_list(const char *path_info, const char *dir_cores,
      const char *exts, bool show_hidden_files,
      bool enable_cache, bool *cache_supported);

bool core_info_get_list(core_info_list_t **core);

/* Returns number of installed cores */
size_t core_info_count(void);

bool core_info_list_update_missing_firmware(core_info_ctx_firmware_t *info,
      bool *set_missing_bios);

bool core_info_find(const char *core_path,
      core_info_t **core_info);

bool core_info_load(const char *core_path);

bool core_info_database_supports_content_path(const char *database_path, const char *path);

bool core_info_database_match_archive_member(const char *database_path);

void core_info_qsort(core_info_list_t *core_info_list, enum core_info_list_qsort_type qsort_type);

bool core_info_list_get_info(core_info_list_t *core_info_list,
      core_info_t *out_info, const char *core_path);

bool core_info_hw_api_supported(core_info_t *info);

/* Sets 'locked' status of specified core
 * > Returns true if successful
 * > Like all functions that access the cached
 *   core info list this is *not* thread safe */
bool core_info_set_core_lock(const char *core_path, bool lock);
/* Fetches 'locked' status of specified core
 * > If 'validate_path' is 'true', will search
 *   cached core info list for a corresponding
 *   'sanitised' core file path. This is *not*
 *   thread safe
 * > If 'validate_path' is 'false', performs a
 *   direct filesystem check. This *is* thread
 *   safe, but validity of specified core path
 *   must be checked externally */
bool core_info_get_core_lock(const char *core_path, bool validate_path);

bool core_info_core_file_id_is_equal(const char *core_path_a, const char *core_path_b);

/* When called, generates a temporary file
 * that will force an info cache refresh the
 * next time that core info is initialised with
 * caching enabled */
bool core_info_cache_force_refresh(const char *path_info);

RETRO_END_DECLS

#endif /* CORE_INFO_H_ */
