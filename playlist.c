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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <libretro.h>
#include <boolean.h>
#include <retro_assert.h>
#include <retro_miscellaneous.h>
#include <compat/posix_string.h>
#include <string/stdstring.h>
#include <streams/interface_stream.h>
#include <file/file_path.h>
#include <file/archive_file.h>
#include <lists/string_list.h>
#include <formats/rjson.h>
#include <array/rbuf.h>

#include "playlist.h"
#include "verbosity.h"
#include "file_path_special.h"
#include "core_info.h"

#if defined(ANDROID)
#include "play_feature_delivery/play_feature_delivery.h"
#endif

#ifndef PLAYLIST_ENTRIES
#define PLAYLIST_ENTRIES 6
#endif

#define WINDOWS_PATH_DELIMITER '\\'
#define POSIX_PATH_DELIMITER '/'

#ifdef _WIN32
#define LOCAL_FILE_SYSTEM_PATH_DELIMITER WINDOWS_PATH_DELIMITER
#define USING_WINDOWS_FILE_SYSTEM
#else
#define LOCAL_FILE_SYSTEM_PATH_DELIMITER POSIX_PATH_DELIMITER
#define USING_POSIX_FILE_SYSTEM
#endif

/* Holds all configuration parameters required
 * to repeat a manual content scan for a
 * previously manual-scan-generated playlist */
typedef struct
{
   char *content_dir;
   char *file_exts;
   char *dat_file_path;
   bool search_recursively;
   bool search_archives;
   bool filter_dat_content;
} playlist_manual_scan_record_t;

struct content_playlist
{
   char *default_core_path;
   char *default_core_name;
   char *base_content_directory;

   struct playlist_entry *entries;

   playlist_manual_scan_record_t scan_record; /* ptr alignment */
   playlist_config_t config;                  /* size_t alignment */

   enum playlist_label_display_mode label_display_mode;
   enum playlist_thumbnail_mode right_thumbnail_mode;
   enum playlist_thumbnail_mode left_thumbnail_mode;
   enum playlist_sort_mode sort_mode;

   bool modified;
   bool old_format;
   bool compressed;
   bool cached_external;
};

typedef struct
{
   struct playlist_entry *current_entry;
   char **current_string_val;
   unsigned *current_entry_uint_val;
   enum playlist_label_display_mode *current_meta_label_display_mode_val;
   enum playlist_thumbnail_mode *current_meta_thumbnail_mode_val;
   enum playlist_sort_mode *current_meta_sort_mode_val;
   bool *current_meta_bool_val;
   playlist_t *playlist;

   unsigned array_depth;
   unsigned object_depth;

   bool in_items;
   bool in_subsystem_roms;
   bool capacity_exceeded;
   bool out_of_memory;
} JSONContext;

/* TODO/FIXME - global state - perhaps move outside this file */
static playlist_t *playlist_cached = NULL;

typedef int (playlist_sort_fun_t)(
      const struct playlist_entry *a,
      const struct playlist_entry *b);

/* TODO/FIXME - hack for allowing the explore view to switch 
 * over to a playlist item */
void playlist_set_cached_external(playlist_t* pl)
{
   playlist_free_cached();
   if (!pl)
      return;

   playlist_cached = pl;
   playlist_cached->cached_external = true;
}

/* Convenience function: copies specified playlist
 * path to specified playlist configuration object */
void playlist_config_set_path(playlist_config_t *config, const char *path)
{
   if (!config)
      return;

   if (!string_is_empty(path))
      strlcpy(config->path, path, sizeof(config->path));
   else
      config->path[0] = '\0';
}

/* Convenience function: copies base content directory
 * path to specified playlist configuration object.
 * Also sets autofix_paths boolean, depending on base 
 * content directory value */
void playlist_config_set_base_content_directory(
      playlist_config_t* config, const char* path)
{
   if (!config)
      return;

   config->autofix_paths = !string_is_empty(path);
   if (config->autofix_paths)
      strlcpy(config->base_content_directory, path,
            sizeof(config->base_content_directory));
   else
      config->base_content_directory[0] = '\0';
}


/* Creates a copy of the specified playlist configuration.
 * Returns false in the event of an error */
bool playlist_config_copy(const playlist_config_t *src,
      playlist_config_t *dst)
{
   if (!src || !dst)
      return false;

   strlcpy(dst->path, src->path, sizeof(dst->path));
   strlcpy(dst->base_content_directory, src->base_content_directory,
         sizeof(dst->base_content_directory));

   dst->capacity            = src->capacity;
   dst->old_format          = src->old_format;
   dst->compress            = src->compress;
   dst->fuzzy_archive_match = src->fuzzy_archive_match;
   dst->autofix_paths       = src->autofix_paths;

   return true;
}

/* Returns internal playlist configuration object
 * of specified playlist.
 * Returns NULL it the event of an error. */
playlist_config_t *playlist_get_config(playlist_t *playlist)
{
   if (!playlist)
      return NULL;

   return &playlist->config;
}

static void path_replace_base_path_and_convert_to_local_file_system(
      char *out_path, const char *in_path,
      const char *in_oldrefpath, const char *in_refpath,
      size_t size)
{
   size_t in_oldrefpath_length = strlen(in_oldrefpath);
   size_t in_refpath_length    = strlen(in_refpath);

   /* If entry path is inside playlist base path,
    * replace it with new base content directory */
   if (string_starts_with_size(in_path, in_oldrefpath, in_oldrefpath_length))
   {
      memcpy(out_path, in_refpath, in_refpath_length);
      memcpy(out_path + in_refpath_length, in_path + in_oldrefpath_length,
            strlen(in_path) - in_oldrefpath_length + 1);

#ifdef USING_WINDOWS_FILE_SYSTEM
      /* If we are running under a Windows filesystem,
       * '/' characters are not allowed anywhere. 
       * We replace with '\' and hope for the best... */
      string_replace_all_chars(out_path,
            POSIX_PATH_DELIMITER, WINDOWS_PATH_DELIMITER);
#endif

#ifdef USING_POSIX_FILE_SYSTEM
      /* Under POSIX filesystem, we replace '\' characters with '/' */
      string_replace_all_chars(out_path,
            WINDOWS_PATH_DELIMITER, POSIX_PATH_DELIMITER);
#endif
   }
   else
      strlcpy(out_path, in_path, size);
}

/* Generates a case insensitive hash for the
 * specified path string */
static uint32_t playlist_path_hash(const char *path)
{
   unsigned char c;
   uint32_t hash = (uint32_t)0x811c9dc5;
   while ((c = (unsigned char)*(path++)) != '\0')
      hash = ((hash * (uint32_t)0x01000193) ^ (uint32_t)((c >= 'A' && c <= 'Z') ? (c | 0x20) : c));
   return (hash ? hash : 1);
}

static void playlist_path_id_free(playlist_path_id_t *path_id)
{
   if (!path_id)
      return;

   if (path_id->archive_path &&
       (path_id->archive_path != path_id->real_path))
      free(path_id->archive_path);

   if (path_id->real_path)
      free(path_id->real_path);

   free(path_id);
}

static playlist_path_id_t *playlist_path_id_init(const char *path)
{
   playlist_path_id_t *path_id = (playlist_path_id_t*)malloc(sizeof(*path_id));
   const char *archive_delim   = NULL;
   char real_path[PATH_MAX_LENGTH];

   if (!path_id)
      return NULL;

   path_id->real_path         = NULL;
   path_id->archive_path      = NULL;
   path_id->real_path_hash    = 0;
   path_id->archive_path_hash = 0;
   path_id->is_archive        = false;
   path_id->is_in_archive     = false;

   if (string_is_empty(path))
      return path_id;

   /* Get real path */
   strlcpy(real_path, path, sizeof(real_path));
   playlist_resolve_path(PLAYLIST_SAVE, false, real_path,
         sizeof(real_path));

   path_id->real_path      = strdup(real_path);
   path_id->real_path_hash = playlist_path_hash(real_path);

   /* Check archive status */
   path_id->is_archive     = path_is_compressed_file(real_path);
   archive_delim           = path_get_archive_delim(real_path);

   /* If path refers to a file inside an archive,
    * extract the path of the parent archive */
   if (archive_delim)
   {
      size_t len                         = (1 + archive_delim - real_path);
      char archive_path[PATH_MAX_LENGTH] = {0};

      len = (len < PATH_MAX_LENGTH) ? len : PATH_MAX_LENGTH;
      strlcpy(archive_path, real_path, len * sizeof(char));

      path_id->archive_path      = strdup(archive_path);
      path_id->archive_path_hash = playlist_path_hash(archive_path);
      path_id->is_in_archive     = true;
   }
   else if (path_id->is_archive)
   {
      path_id->archive_path      = path_id->real_path;
      path_id->archive_path_hash = path_id->real_path_hash;
   }

   return path_id;
}

/**
 * playlist_path_equal:
 * @real_path           : 'Real' search path, generated by path_resolve_realpath()
 * @entry_path          : Existing playlist entry 'path' value
 *
 * Returns 'true' if real_path matches entry_path
 * (Taking into account relative paths, case insensitive
 * filesystems, 'incomplete' archive paths)
 **/
static bool playlist_path_equal(const char *real_path,
      const char *entry_path, const playlist_config_t *config)
{
   bool real_path_is_compressed;
   bool entry_real_path_is_compressed;
   char entry_real_path[PATH_MAX_LENGTH];

   /* Sanity check */
   if (string_is_empty(real_path)  ||
       string_is_empty(entry_path) ||
       !config)
      return false;

   /* Get entry 'real' path */
   strlcpy(entry_real_path, entry_path, sizeof(entry_real_path));
   path_resolve_realpath(entry_real_path, sizeof(entry_real_path), true);

   if (string_is_empty(entry_real_path))
      return false;

   /* First pass comparison */
#ifdef _WIN32
   /* Handle case-insensitive operating systems*/
   if (string_is_equal_noncase(real_path, entry_real_path))
      return true;
#else
   if (string_is_equal(real_path, entry_real_path))
      return true;
#endif

#ifdef RARCH_INTERNAL
   /* If fuzzy matching is disabled, we can give up now */
   if (!config->fuzzy_archive_match)
      return false;
#endif

   /* If we reach this point, we have to work
    * harder...
    * Need to handle a rather awkward archive file
    * case where:
    * - playlist path contains a properly formatted
    *   [archive_path][delimiter][rom_file]
    * - search path is just [archive_path]
    * ...or vice versa.
    * This pretty much always happens when a playlist
    * is generated via scan content (which handles the
    * archive paths correctly), but the user subsequently
    * loads an archive file via the command line or some
    * external launcher (where the [delimiter][rom_file]
    * part is almost always omitted) */
   real_path_is_compressed       = path_is_compressed_file(real_path);
   entry_real_path_is_compressed = path_is_compressed_file(entry_real_path);

   if ((real_path_is_compressed  && !entry_real_path_is_compressed) ||
       (!real_path_is_compressed && entry_real_path_is_compressed))
   {
      const char *compressed_path_a  = real_path_is_compressed ? real_path       : entry_real_path;
      const char *full_path          = real_path_is_compressed ? entry_real_path : real_path;
      const char *delim              = path_get_archive_delim(full_path);

      if (delim)
      {
         char compressed_path_b[PATH_MAX_LENGTH] = {0};
         unsigned len = (unsigned)(1 + delim - full_path);

         strlcpy(compressed_path_b, full_path,
               (len < PATH_MAX_LENGTH ? len : PATH_MAX_LENGTH) * sizeof(char));

#ifdef _WIN32
         /* Handle case-insensitive operating systems*/
         if (string_is_equal_noncase(compressed_path_a, compressed_path_b))
            return true;
#else
         if (string_is_equal(compressed_path_a, compressed_path_b))
            return true;
#endif
      }
   }

   return false;
}

/**
 * playlist_path_matches_entry:
 * @path_id           : Path identity, containing 'real' path,
 *                      hash and archive status information
 * @entry             : Playlist entry to compare with path_id
 *
 * Returns 'true' if 'path_id' matches path information
 * contained in specified 'entry'. Will update path_id
 * cache inside specified 'entry', if not already present.
 **/
static bool playlist_path_matches_entry(playlist_path_id_t *path_id,
      struct playlist_entry *entry, const playlist_config_t *config)
{
   /* Sanity check */
   if (!path_id ||
       !entry ||
       !config)
      return false;

   /* Check whether entry contains a path ID cache */
   if (!entry->path_id)
   {
      entry->path_id = playlist_path_id_init(entry->path);
      if (!entry->path_id)
         return false;
   }

   /* Ensure we have valid real_path strings */
   if (string_is_empty(path_id->real_path) ||
       string_is_empty(entry->path_id->real_path))
      return false;

   /* First pass comparison */
   if (path_id->real_path_hash ==
         entry->path_id->real_path_hash)
   {
#ifdef _WIN32
      /* Handle case-insensitive operating systems*/
      if (string_is_equal_noncase(path_id->real_path,
            entry->path_id->real_path))
         return true;
#else
      if (string_is_equal(path_id->real_path,
            entry->path_id->real_path))
         return true;
#endif
   }

#ifdef RARCH_INTERNAL
   /* If fuzzy matching is disabled, we can give up now */
   if (!config->fuzzy_archive_match)
      return false;
#endif

   /* If we reach this point, we have to work
    * harder...
    * Need to handle a rather awkward archive file
    * case where:
    * - playlist path contains a properly formatted
    *   [archive_path][delimiter][rom_file]
    * - search path is just [archive_path]
    * ...or vice versa.
    * This pretty much always happens when a playlist
    * is generated via scan content (which handles the
    * archive paths correctly), but the user subsequently
    * loads an archive file via the command line or some
    * external launcher (where the [delimiter][rom_file]
    * part is almost always omitted) */
   if (((path_id->is_archive        && !path_id->is_in_archive)        && entry->path_id->is_in_archive) ||
       ((entry->path_id->is_archive && !entry->path_id->is_in_archive) && path_id->is_in_archive))
   {
      /* Ensure we have valid parent archive path
       * strings */
      if (string_is_empty(path_id->archive_path) ||
          string_is_empty(entry->path_id->archive_path))
         return false;

      if (path_id->archive_path_hash ==
            entry->path_id->archive_path_hash)
      {
#ifdef _WIN32
         /* Handle case-insensitive operating systems*/
         if (string_is_equal_noncase(path_id->archive_path,
               entry->path_id->archive_path))
            return true;
#else
         if (string_is_equal(path_id->archive_path,
               entry->path_id->archive_path))
            return true;
#endif
      }
   }

   return false;
}

/**
 * playlist_core_path_equal:
 * @real_core_path  : 'Real' search path, generated by path_resolve_realpath()
 * @entry_core_path : Existing playlist entry 'core path' value
 * @config          : Playlist config parameters
 *
 * Returns 'true' if real_core_path matches entry_core_path
 * (Taking into account relative paths, case insensitive
 * filesystems)
 **/
static bool playlist_core_path_equal(const char *real_core_path, const char *entry_core_path, const playlist_config_t *config)
{
   char entry_real_core_path[PATH_MAX_LENGTH];

   /* Sanity check */
   if (string_is_empty(real_core_path) || string_is_empty(entry_core_path))
      return false;

   /* Get entry 'real' core path */
   strlcpy(entry_real_core_path, entry_core_path, sizeof(entry_real_core_path));
   if (!string_is_equal(entry_real_core_path, FILE_PATH_DETECT) &&
       !string_is_equal(entry_real_core_path, FILE_PATH_BUILTIN))
      playlist_resolve_path(PLAYLIST_SAVE, true, entry_real_core_path,
            sizeof(entry_real_core_path));

   if (string_is_empty(entry_real_core_path))
      return false;

#ifdef _WIN32
   /* Handle case-insensitive operating systems*/
   if (string_is_equal_noncase(real_core_path, entry_real_core_path))
      return true;
#else
   if (string_is_equal(real_core_path, entry_real_core_path))
      return true;
#endif

   if (config->autofix_paths &&
       core_info_core_file_id_is_equal(real_core_path, entry_core_path))
      return true;

   return false;
}

uint32_t playlist_get_size(playlist_t *playlist)
{
   if (!playlist)
      return 0;
   return (uint32_t)RBUF_LEN(playlist->entries);
}

char *playlist_get_conf_path(playlist_t *playlist)
{
   if (!playlist)
      return NULL;
   return playlist->config.path;
}

/**
 * playlist_get_index:
 * @playlist            : Playlist handle.
 * @idx                 : Index of playlist entry.
 * @path                : Path of playlist entry.
 * @core_path           : Core path of playlist entry.
 * @core_name           : Core name of playlist entry.
 *
 * Gets values of playlist index:
 **/
void playlist_get_index(playlist_t *playlist,
      size_t idx,
      const struct playlist_entry **entry)
{
   if (!playlist || !entry || (idx >= RBUF_LEN(playlist->entries)))
      return;

   *entry = &playlist->entries[idx];
}

/**
 * playlist_free_entry:
 * @entry               : Playlist entry handle.
 *
 * Frees playlist entry.
 **/
static void playlist_free_entry(struct playlist_entry *entry)
{
   if (!entry)
      return;

   if (entry->path)
      free(entry->path);
   if (entry->label)
      free(entry->label);
   if (entry->core_path)
      free(entry->core_path);
   if (entry->core_name)
      free(entry->core_name);
   if (entry->db_name)
      free(entry->db_name);
   if (entry->crc32)
      free(entry->crc32);
   if (entry->subsystem_ident)
      free(entry->subsystem_ident);
   if (entry->subsystem_name)
      free(entry->subsystem_name);
   if (entry->runtime_str)
      free(entry->runtime_str);
   if (entry->last_played_str)
      free(entry->last_played_str);
   if (entry->subsystem_roms)
      string_list_free(entry->subsystem_roms);
   if (entry->path_id)
      playlist_path_id_free(entry->path_id);

   entry->path      = NULL;
   entry->label     = NULL;
   entry->core_path = NULL;
   entry->core_name = NULL;
   entry->db_name   = NULL;
   entry->crc32     = NULL;
   entry->subsystem_ident = NULL;
   entry->subsystem_name = NULL;
   entry->runtime_str = NULL;
   entry->last_played_str = NULL;
   entry->subsystem_roms = NULL;
   entry->path_id = NULL;
   entry->runtime_status = PLAYLIST_RUNTIME_UNKNOWN;
   entry->runtime_hours = 0;
   entry->runtime_minutes = 0;
   entry->runtime_seconds = 0;
   entry->last_played_year = 0;
   entry->last_played_month = 0;
   entry->last_played_day = 0;
   entry->last_played_hour = 0;
   entry->last_played_minute = 0;
   entry->last_played_second = 0;
}

/**
 * playlist_delete_index:
 * @playlist            : Playlist handle.
 * @idx                 : Index of playlist entry.
 *
 * Delete the entry at the index:
 **/
void playlist_delete_index(playlist_t *playlist,
      size_t idx)
{
   size_t len;
   struct playlist_entry *entry_to_delete;

   if (!playlist)
      return;

   len = RBUF_LEN(playlist->entries);
   if (idx >= len)
      return;

   /* Free unwanted entry */
   entry_to_delete = (struct playlist_entry *)(playlist->entries + idx);
   if (entry_to_delete)
      playlist_free_entry(entry_to_delete);

   /* Shift remaining entries to fill the gap */
   memmove(playlist->entries + idx, playlist->entries + idx + 1,
         (len - 1 - idx) * sizeof(struct playlist_entry));

   RBUF_RESIZE(playlist->entries, len - 1);

   playlist->modified = true;
}

/**
 * playlist_delete_by_path:
 * @playlist            : Playlist handle.
 * @search_path         : Content path.
 *
 * Deletes all entries with content path
 * matching 'search_path'
 **/
void playlist_delete_by_path(playlist_t *playlist,
      const char *search_path)
{
   playlist_path_id_t *path_id = NULL;
   size_t i                    = 0;

   if (!playlist || string_is_empty(search_path))
      return;

   path_id = playlist_path_id_init(search_path);
   if (!path_id)
      return;

   while (i < RBUF_LEN(playlist->entries))
   {
      if (!playlist_path_matches_entry(path_id,
            &playlist->entries[i], &playlist->config))
      {
         i++;
         continue;
      }

      /* Paths are equal - delete entry */
      playlist_delete_index(playlist, i);

      /* Entries are shifted up by the delete
       * operation - *do not* increment i */
   }

   playlist_path_id_free(path_id);
}

void playlist_get_index_by_path(playlist_t *playlist,
      const char *search_path,
      const struct playlist_entry **entry)
{
   playlist_path_id_t *path_id = NULL;
   size_t i, len;

   if (!playlist || !entry || string_is_empty(search_path))
      return;

   path_id = playlist_path_id_init(search_path);
   if (!path_id)
      return;

   for (i = 0, len = RBUF_LEN(playlist->entries); i < len; i++)
   {
      if (!playlist_path_matches_entry(path_id,
            &playlist->entries[i], &playlist->config))
         continue;

      *entry = &playlist->entries[i];
      break;
   }

   playlist_path_id_free(path_id);
}

bool playlist_entry_exists(playlist_t *playlist,
      const char *path)
{
   playlist_path_id_t *path_id = NULL;
   size_t i, len;

   if (!playlist || string_is_empty(path))
      return false;

   path_id = playlist_path_id_init(path);
   if (!path_id)
      return false;

   for (i = 0, len = RBUF_LEN(playlist->entries); i < len; i++)
   {
      if (playlist_path_matches_entry(path_id,
            &playlist->entries[i], &playlist->config))
      {
         playlist_path_id_free(path_id);
         return true;
      }
   }

   playlist_path_id_free(path_id);
   return false;
}

void playlist_update(playlist_t *playlist, size_t idx,
      const struct playlist_entry *update_entry)
{
   struct playlist_entry *entry = NULL;

   if (!playlist || idx >= RBUF_LEN(playlist->entries))
      return;

   entry            = &playlist->entries[idx];

   if (update_entry->path && (update_entry->path != entry->path))
   {
      if (entry->path)
         free(entry->path);
      entry->path        = strdup(update_entry->path);

      if (entry->path_id)
      {
         playlist_path_id_free(entry->path_id);
         entry->path_id  = NULL;
      }

      playlist->modified = true;
   }

   if (update_entry->label && (update_entry->label != entry->label))
   {
      if (entry->label)
         free(entry->label);
      entry->label       = strdup(update_entry->label);
      playlist->modified = true;
   }

   if (update_entry->core_path && (update_entry->core_path != entry->core_path))
   {
      if (entry->core_path)
         free(entry->core_path);
      entry->core_path   = NULL;
      entry->core_path   = strdup(update_entry->core_path);
      playlist->modified = true;
   }

   if (update_entry->core_name && (update_entry->core_name != entry->core_name))
   {
      if (entry->core_name)
         free(entry->core_name);
      entry->core_name   = strdup(update_entry->core_name);
      playlist->modified = true;
   }

   if (update_entry->db_name && (update_entry->db_name != entry->db_name))
   {
      if (entry->db_name)
         free(entry->db_name);
      entry->db_name     = strdup(update_entry->db_name);
      playlist->modified = true;
   }

   if (update_entry->crc32 && (update_entry->crc32 != entry->crc32))
   {
      if (entry->crc32)
         free(entry->crc32);
      entry->crc32       = strdup(update_entry->crc32);
      playlist->modified = true;
   }
}

void playlist_update_runtime(playlist_t *playlist, size_t idx,
      const struct playlist_entry *update_entry,
      bool register_update)
{
   struct playlist_entry *entry = NULL;

   if (!playlist || idx >= RBUF_LEN(playlist->entries))
      return;

   entry            = &playlist->entries[idx];

   if (update_entry->path && (update_entry->path != entry->path))
   {
      if (entry->path)
         free(entry->path);
      entry->path        = strdup(update_entry->path);

      if (entry->path_id)
      {
         playlist_path_id_free(entry->path_id);
         entry->path_id  = NULL;
      }

      playlist->modified = playlist->modified || register_update;
   }

   if (update_entry->core_path && (update_entry->core_path != entry->core_path))
   {
      if (entry->core_path)
         free(entry->core_path);
      entry->core_path   = NULL;
      entry->core_path   = strdup(update_entry->core_path);
      playlist->modified = playlist->modified || register_update;
   }

   if (update_entry->runtime_status != entry->runtime_status)
   {
      entry->runtime_status = update_entry->runtime_status;
      playlist->modified = playlist->modified || register_update;
   }

   if (update_entry->runtime_hours != entry->runtime_hours)
   {
      entry->runtime_hours = update_entry->runtime_hours;
      playlist->modified = playlist->modified || register_update;
   }

   if (update_entry->runtime_minutes != entry->runtime_minutes)
   {
      entry->runtime_minutes = update_entry->runtime_minutes;
      playlist->modified = playlist->modified || register_update;
   }

   if (update_entry->runtime_seconds != entry->runtime_seconds)
   {
      entry->runtime_seconds = update_entry->runtime_seconds;
      playlist->modified = playlist->modified || register_update;
   }

   if (update_entry->last_played_year != entry->last_played_year)
   {
      entry->last_played_year = update_entry->last_played_year;
      playlist->modified = playlist->modified || register_update;
   }

   if (update_entry->last_played_month != entry->last_played_month)
   {
      entry->last_played_month = update_entry->last_played_month;
      playlist->modified = playlist->modified || register_update;
   }

   if (update_entry->last_played_day != entry->last_played_day)
   {
      entry->last_played_day = update_entry->last_played_day;
      playlist->modified = playlist->modified || register_update;
   }

   if (update_entry->last_played_hour != entry->last_played_hour)
   {
      entry->last_played_hour = update_entry->last_played_hour;
      playlist->modified = playlist->modified || register_update;
   }

   if (update_entry->last_played_minute != entry->last_played_minute)
   {
      entry->last_played_minute = update_entry->last_played_minute;
      playlist->modified = playlist->modified || register_update;
   }

   if (update_entry->last_played_second != entry->last_played_second)
   {
      entry->last_played_second = update_entry->last_played_second;
      playlist->modified = playlist->modified || register_update;
   }

   if (update_entry->runtime_str && (update_entry->runtime_str != entry->runtime_str))
   {
      if (entry->runtime_str)
         free(entry->runtime_str);
      entry->runtime_str = NULL;
      entry->runtime_str = strdup(update_entry->runtime_str);
      playlist->modified = playlist->modified || register_update;
   }

   if (update_entry->last_played_str && (update_entry->last_played_str != entry->last_played_str))
   {
      if (entry->last_played_str)
         free(entry->last_played_str);
      entry->last_played_str = NULL;
      entry->last_played_str = strdup(update_entry->last_played_str);
      playlist->modified = playlist->modified || register_update;
   }
}

bool playlist_push_runtime(playlist_t *playlist,
      const struct playlist_entry *entry)
{
   playlist_path_id_t *path_id = NULL;
   size_t i, len;
   char real_core_path[PATH_MAX_LENGTH];

   if (!playlist || !entry)
      goto error;

   if (string_is_empty(entry->core_path))
   {
      RARCH_ERR("cannot push NULL or empty core path into the playlist.\n");
      goto error;
   }

   /* Get path ID */
   if (!(path_id = playlist_path_id_init(entry->path)))
      goto error;

   /* Get 'real' core path */
   strlcpy(real_core_path, entry->core_path, sizeof(real_core_path));
   if (!string_is_equal(real_core_path, FILE_PATH_DETECT) &&
       !string_is_equal(real_core_path, FILE_PATH_BUILTIN))
      playlist_resolve_path(PLAYLIST_SAVE, true, real_core_path,
             sizeof(real_core_path));

   if (string_is_empty(real_core_path))
   {
      RARCH_ERR("cannot push NULL or empty core path into the playlist.\n");
      goto error;
   }

   len = RBUF_LEN(playlist->entries);
   for (i = 0; i < len; i++)
   {
      struct playlist_entry tmp;
      bool equal_path  = (string_is_empty(path_id->real_path) &&
            string_is_empty(playlist->entries[i].path));

      equal_path       = equal_path || playlist_path_matches_entry(
            path_id, &playlist->entries[i], &playlist->config);

      if (!equal_path)
         continue;

      /* Core name can have changed while still being the same core.
       * Differentiate based on the core path only. */
      if (!playlist_core_path_equal(real_core_path, playlist->entries[i].core_path, &playlist->config))
         continue;

      /* If top entry, we don't want to push a new entry since
       * the top and the entry to be pushed are the same. */
      if (i == 0)
         goto error;

      /* Seen it before, bump to top. */
      tmp = playlist->entries[i];
      memmove(playlist->entries + 1, playlist->entries,
            i * sizeof(struct playlist_entry));
      playlist->entries[0] = tmp;

      goto success;
   }

   if (playlist->config.capacity == 0)
      goto error;

   if (len == playlist->config.capacity)
   {
      struct playlist_entry *last_entry = &playlist->entries[len - 1];
      playlist_free_entry(last_entry);
      len--;
   }
   else
   {
      /* Allocate memory to fit one more item and resize the buffer */
      if (!RBUF_TRYFIT(playlist->entries, len + 1))
         goto error; /* out of memory */
      RBUF_RESIZE(playlist->entries, len + 1);
   }

   if (playlist->entries)
   {
      memmove(playlist->entries + 1, playlist->entries,
            len * sizeof(struct playlist_entry));

      playlist->entries[0].path            = NULL;
      playlist->entries[0].core_path       = NULL;

      if (!string_is_empty(path_id->real_path))
         playlist->entries[0].path         = strdup(path_id->real_path);
      playlist->entries[0].path_id         = path_id;
      path_id                              = NULL;

      if (!string_is_empty(real_core_path))
         playlist->entries[0].core_path    = strdup(real_core_path);

      playlist->entries[0].runtime_status = entry->runtime_status;
      playlist->entries[0].runtime_hours = entry->runtime_hours;
      playlist->entries[0].runtime_minutes = entry->runtime_minutes;
      playlist->entries[0].runtime_seconds = entry->runtime_seconds;
      playlist->entries[0].last_played_year = entry->last_played_year;
      playlist->entries[0].last_played_month = entry->last_played_month;
      playlist->entries[0].last_played_day = entry->last_played_day;
      playlist->entries[0].last_played_hour = entry->last_played_hour;
      playlist->entries[0].last_played_minute = entry->last_played_minute;
      playlist->entries[0].last_played_second = entry->last_played_second;

      playlist->entries[0].runtime_str        = NULL;
      playlist->entries[0].last_played_str    = NULL;

      if (!string_is_empty(entry->runtime_str))
         playlist->entries[0].runtime_str     = strdup(entry->runtime_str);
      if (!string_is_empty(entry->last_played_str))
         playlist->entries[0].last_played_str = strdup(entry->last_played_str);
   }

success:
   if (path_id)
      playlist_path_id_free(path_id);
   playlist->modified = true;
   return true;

error:
   if (path_id)
      playlist_path_id_free(path_id);
   return false;
}

/**
 * playlist_resolve_path:
 * @mode      : PLAYLIST_LOAD or PLAYLIST_SAVE
 * @is_core   : Set true if path to be resolved is a core file
 * @path      : The path to be modified
 *
 * Resolves the path of an item, such as the content path or path to the core, to a format
 * appropriate for saving or loading depending on the @mode parameter
 *
 * Can be platform specific. File paths for saving can be abbreviated to avoid saving absolute
 * paths, as the base directory (home or application dir) may change after each subsequent
 * install (iOS)
**/
void playlist_resolve_path(enum playlist_file_mode mode,
      bool is_core, char *path, size_t len)
{
#ifdef HAVE_COCOATOUCH
   char tmp[PATH_MAX_LENGTH];

   if (mode == PLAYLIST_LOAD)
   {
      fill_pathname_expand_special(tmp, path, sizeof(tmp));
      strlcpy(path, tmp, len);
   }
   else
   {
      /* iOS needs to call realpath here since the call
       * above fails due to possibly buffer related issues.
       * Try to expand the path to ensure that it gets saved
       * correctly. The path can be abbreviated if saving to
       * a playlist from another playlist (ex: content history to favorites)
       */
      char tmp2[PATH_MAX_LENGTH];
      fill_pathname_expand_special(tmp, path, sizeof(tmp));
      realpath(tmp, tmp2);
      fill_pathname_abbreviate_special(path, tmp2, len);
   }
#else
   bool resolve_symlinks = true;

   if (mode == PLAYLIST_LOAD)
      return;

#if defined(ANDROID)
   /* Can't resolve symlinks when dealing with cores
    * installed via play feature delivery, because the
    * source files have non-standard file names (which
    * will not be recognised by regular core handling
    * routines) */
   if (is_core)
      resolve_symlinks = !play_feature_delivery_enabled();
#endif

   path_resolve_realpath(path, len, resolve_symlinks);
#endif
}

/**
 * playlist_content_path_is_valid:
 * @path      : Content path
 *
 * Checks whether specified playlist content path
 * refers to an existent file. Handles all playlist
 * content path 'types' (i.e. can validate paths
 * referencing files inside archives).
 *
 * Returns true if file referenced by content
 * path exists on the host filesystem.
 **/
bool playlist_content_path_is_valid(const char *path)
{
   /* Sanity check */
   if (string_is_empty(path))
      return false;

   /* If content is inside an archive, special
    * handling is required... */
   if (path_contains_compressed_file(path))
   {
      const char *delim                  = path_get_archive_delim(path);
      char archive_path[PATH_MAX_LENGTH] = {0};
      size_t len                         = 0;
      struct string_list *archive_list   = NULL;
      const char *content_file           = NULL;
      bool content_found                 = false;

      if (!delim)
         return false;

      /* Get path of 'parent' archive file */
      len = (size_t)(1 + delim - path);
      strlcpy(archive_path, path,
            (len < PATH_MAX_LENGTH ? len : PATH_MAX_LENGTH) * sizeof(char));

      /* Check if archive itself exists */
      if (!path_is_valid(archive_path))
         return false;

      /* Check if file exists inside archive */
      archive_list = file_archive_get_file_list(archive_path, NULL);

      if (!archive_list)
         return false;

      /* > Get playlist entry content file name
       *   (sans archive file path) */
      content_file = delim;
      content_file++;

      if (!string_is_empty(content_file))
      {
         size_t i;

         /* > Loop over archive file contents */
         for (i = 0; i < archive_list->size; i++)
         {
            const char *archive_file = archive_list->elems[i].data;

            if (string_is_empty(archive_file))
               continue;

            if (string_is_equal(content_file, archive_file))
            {
               content_found = true;
               break;
            }
         }
      }

      /* Clean up */
      string_list_free(archive_list);

      return content_found;
   }
   /* This is a 'normal' path - just check if
    * it's valid */
   else
      return path_is_valid(path);
}

/**
 * playlist_push:
 * @playlist        	   : Playlist handle.
 *
 * Push entry to top of playlist.
 **/
bool playlist_push(playlist_t *playlist,
      const struct playlist_entry *entry)
{
   size_t i, len;
   char real_core_path[PATH_MAX_LENGTH];
   playlist_path_id_t *path_id = NULL;
   const char *core_name       = entry->core_name;
   bool entry_updated          = false;

   if (!playlist || !entry)
      goto error;

   if (string_is_empty(entry->core_path))
   {
      RARCH_ERR("cannot push NULL or empty core path into the playlist.\n");
      goto error;
   }

   /* Get path ID */
   if (!(path_id = playlist_path_id_init(entry->path)))
      goto error;

   /* Get 'real' core path */
   strlcpy(real_core_path, entry->core_path, sizeof(real_core_path));
   if (!string_is_equal(real_core_path, FILE_PATH_DETECT) &&
       !string_is_equal(real_core_path, FILE_PATH_BUILTIN))
      playlist_resolve_path(PLAYLIST_SAVE, true, real_core_path,
             sizeof(real_core_path));

   if (string_is_empty(real_core_path))
   {
      RARCH_ERR("cannot push NULL or empty core path into the playlist.\n");
      goto error;
   }

   if (string_is_empty(core_name))
   {
      static char base_path[255] = {0};
      fill_pathname_base(base_path, real_core_path, sizeof(base_path));
      path_remove_extension(base_path);

      core_name = base_path;

      if (string_is_empty(core_name))
      {
         RARCH_ERR("cannot push NULL or empty core name into the playlist.\n");
         goto error;
      }
   }

   len = RBUF_LEN(playlist->entries);
   for (i = 0; i < len; i++)
   {
      struct playlist_entry tmp;
      bool equal_path  = (string_is_empty(path_id->real_path) &&
            string_is_empty(playlist->entries[i].path));

      equal_path       = equal_path || playlist_path_matches_entry(
            path_id, &playlist->entries[i], &playlist->config);

      if (!equal_path)
         continue;

      /* Core name can have changed while still being the same core.
       * Differentiate based on the core path only. */
      if (!playlist_core_path_equal(real_core_path, playlist->entries[i].core_path, &playlist->config))
         continue;

      if (     !string_is_empty(entry->subsystem_ident)
            && !string_is_empty(playlist->entries[i].subsystem_ident)
            && !string_is_equal(playlist->entries[i].subsystem_ident, entry->subsystem_ident))
         continue;

      if (      string_is_empty(entry->subsystem_ident)
            && !string_is_empty(playlist->entries[i].subsystem_ident))
         continue;

      if (    !string_is_empty(entry->subsystem_ident)
            && string_is_empty(playlist->entries[i].subsystem_ident))
         continue;

      if (     !string_is_empty(entry->subsystem_name)
            && !string_is_empty(playlist->entries[i].subsystem_name)
            && !string_is_equal(playlist->entries[i].subsystem_name, entry->subsystem_name))
         continue;

      if (      string_is_empty(entry->subsystem_name)
            && !string_is_empty(playlist->entries[i].subsystem_name))
         continue;

      if (     !string_is_empty(entry->subsystem_name)
            &&  string_is_empty(playlist->entries[i].subsystem_name))
         continue;

      if (entry->subsystem_roms)
      {
         unsigned j;
         const struct string_list *roms = playlist->entries[i].subsystem_roms;
         bool                   unequal = false;

         if (entry->subsystem_roms->size != roms->size)
            continue;

         for (j = 0; j < entry->subsystem_roms->size; j++)
         {
            char real_rom_path[PATH_MAX_LENGTH];

            if (!string_is_empty(entry->subsystem_roms->elems[j].data))
            {
               strlcpy(real_rom_path, entry->subsystem_roms->elems[j].data, sizeof(real_rom_path));
               path_resolve_realpath(real_rom_path, sizeof(real_rom_path), true);
            }
            else
               real_rom_path[0] = '\0';

            if (!playlist_path_equal(real_rom_path, roms->elems[j].data,
                     &playlist->config))
            {
               unequal = true;
               break;
            }
         }

         if (unequal)
            continue;
      }

      if (playlist->entries[i].entry_slot != entry->entry_slot)
      {
         playlist->entries[i].entry_slot  = entry->entry_slot;
         entry_updated                    = true;
      }

      /* If content was previously loaded via file browser
       * or command line, certain entry values will be missing.
       * If we are now loading the same content from a playlist,
       * fill in any blanks */
      if (!playlist->entries[i].label && !string_is_empty(entry->label))
      {
         playlist->entries[i].label       = strdup(entry->label);
         entry_updated                    = true;
      }
      if (!playlist->entries[i].crc32 && !string_is_empty(entry->crc32))
      {
         playlist->entries[i].crc32       = strdup(entry->crc32);
         entry_updated                    = true;
      }
      if (!playlist->entries[i].db_name && !string_is_empty(entry->db_name))
      {
         playlist->entries[i].db_name     = strdup(entry->db_name);
         entry_updated                    = true;
      }

      /* If top entry, we don't want to push a new entry since
       * the top and the entry to be pushed are the same. */
      if (i == 0)
      {
         if (entry_updated)
            goto success;

         goto error;
      }

      /* Seen it before, bump to top. */
      tmp = playlist->entries[i];
      memmove(playlist->entries + 1, playlist->entries,
            i * sizeof(struct playlist_entry));
      playlist->entries[0] = tmp;

      goto success;
   }

   if (playlist->config.capacity == 0)
      goto error;

   if (len == playlist->config.capacity)
   {
      struct playlist_entry *last_entry = &playlist->entries[len - 1];
      playlist_free_entry(last_entry);
      len--;
   }
   else
   {
      /* Allocate memory to fit one more item and resize the buffer */
      if (!RBUF_TRYFIT(playlist->entries, len + 1))
         goto error; /* out of memory */
      RBUF_RESIZE(playlist->entries, len + 1);
   }

   if (playlist->entries)
   {
      memmove(playlist->entries + 1, playlist->entries,
            len * sizeof(struct playlist_entry));

      playlist->entries[0].path               = NULL;
      playlist->entries[0].label              = NULL;
      playlist->entries[0].core_path          = NULL;
      playlist->entries[0].core_name          = NULL;
      playlist->entries[0].db_name            = NULL;
      playlist->entries[0].crc32              = NULL;
      playlist->entries[0].subsystem_ident    = NULL;
      playlist->entries[0].subsystem_name     = NULL;
      playlist->entries[0].runtime_str        = NULL;
      playlist->entries[0].last_played_str    = NULL;
      playlist->entries[0].subsystem_roms     = NULL;
      playlist->entries[0].path_id            = NULL;
      playlist->entries[0].runtime_status     = PLAYLIST_RUNTIME_UNKNOWN;
      playlist->entries[0].runtime_hours      = 0;
      playlist->entries[0].runtime_minutes    = 0;
      playlist->entries[0].runtime_seconds    = 0;
      playlist->entries[0].last_played_year   = 0;
      playlist->entries[0].last_played_month  = 0;
      playlist->entries[0].last_played_day    = 0;
      playlist->entries[0].last_played_hour   = 0;
      playlist->entries[0].last_played_minute = 0;
      playlist->entries[0].last_played_second = 0;

      if (!string_is_empty(path_id->real_path))
         playlist->entries[0].path            = strdup(path_id->real_path);
      playlist->entries[0].path_id            = path_id;
      path_id                                 = NULL;

      playlist->entries[0].entry_slot         = entry->entry_slot;

      if (!string_is_empty(entry->label))
         playlist->entries[0].label           = strdup(entry->label);
      if (!string_is_empty(real_core_path))
         playlist->entries[0].core_path       = strdup(real_core_path);
      if (!string_is_empty(core_name))
         playlist->entries[0].core_name       = strdup(core_name);
      if (!string_is_empty(entry->db_name))
         playlist->entries[0].db_name         = strdup(entry->db_name);
      if (!string_is_empty(entry->crc32))
         playlist->entries[0].crc32           = strdup(entry->crc32);
      if (!string_is_empty(entry->subsystem_ident))
         playlist->entries[0].subsystem_ident = strdup(entry->subsystem_ident);
      if (!string_is_empty(entry->subsystem_name))
         playlist->entries[0].subsystem_name  = strdup(entry->subsystem_name);

      if (entry->subsystem_roms)
      {
         union string_list_elem_attr attributes = {0};

         playlist->entries[0].subsystem_roms    = string_list_new();

         for (i = 0; i < entry->subsystem_roms->size; i++)
            string_list_append(playlist->entries[0].subsystem_roms, entry->subsystem_roms->elems[i].data, attributes);
      }
   }

success:
   if (path_id)
      playlist_path_id_free(path_id);
   playlist->modified = true;
   return true;

error:
   if (path_id)
      playlist_path_id_free(path_id);
   return false;
}

void playlist_write_runtime_file(playlist_t *playlist)
{
   size_t i, len;
   intfstream_t *file  = NULL;
   rjsonwriter_t* writer;

   if (!playlist || !playlist->modified)
      return;

   file = intfstream_open_file(playlist->config.path,
         RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!file)
   {
      RARCH_ERR("Failed to write to playlist file: %s\n", playlist->config.path);
      return;
   }

   if (!(writer = rjsonwriter_open_stream(file)))
   {
      RARCH_ERR("Failed to create JSON writer\n");
      goto end;
   }

   rjsonwriter_raw(writer, "{", 1);
   rjsonwriter_raw(writer, "\n", 1);
   rjsonwriter_add_spaces(writer, 2);
   rjsonwriter_add_string(writer, "version");
   rjsonwriter_raw(writer, ":", 1);
   rjsonwriter_raw(writer, " ", 1);
   rjsonwriter_add_string(writer, "1.0");
   rjsonwriter_raw(writer, ",", 1);
   rjsonwriter_raw(writer, "\n", 1);
   rjsonwriter_add_spaces(writer, 2);
   rjsonwriter_add_string(writer, "items");
   rjsonwriter_raw(writer, ":", 1);
   rjsonwriter_raw(writer, " ", 1);
   rjsonwriter_raw(writer, "[", 1);
   rjsonwriter_raw(writer, "\n", 1);

   for (i = 0, len = RBUF_LEN(playlist->entries); i < len; i++)
   {
      rjsonwriter_add_spaces(writer, 4);
      rjsonwriter_raw(writer, "{", 1);

      rjsonwriter_raw(writer, "\n", 1);
      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "path");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_add_string(writer, playlist->entries[i].path);
      rjsonwriter_raw(writer, ",", 1);

      rjsonwriter_raw(writer, "\n", 1);
      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "core_path");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_add_string(writer, playlist->entries[i].core_path);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "runtime_hours");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_rawf(writer, "%u", playlist->entries[i].runtime_hours);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "runtime_minutes");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_rawf(writer, "%u", playlist->entries[i].runtime_minutes);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "runtime_seconds");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_rawf(writer, "%u", playlist->entries[i].runtime_seconds);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "last_played_year");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_rawf(writer, "%u", playlist->entries[i].last_played_year);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "last_played_month");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_rawf(writer, "%u", playlist->entries[i].last_played_month);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "last_played_day");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_rawf(writer, "%u", playlist->entries[i].last_played_day);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "last_played_hour");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_rawf(writer, "%u", playlist->entries[i].last_played_hour);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "last_played_minute");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_rawf(writer, "%u", playlist->entries[i].last_played_minute);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "last_played_second");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_rawf(writer, "%u", playlist->entries[i].last_played_second);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 4);
      rjsonwriter_raw(writer, "}", 1);

      if (i < len - 1)
         rjsonwriter_raw(writer, ",", 1);

      rjsonwriter_raw(writer, "\n", 1);
   }

   rjsonwriter_add_spaces(writer, 2);
   rjsonwriter_raw(writer, "]", 1);
   rjsonwriter_raw(writer, "\n", 1);
   rjsonwriter_raw(writer, "}", 1);
   rjsonwriter_raw(writer, "\n", 1);
   rjsonwriter_free(writer);

   playlist->modified        = false;
   playlist->old_format      = false;
   playlist->compressed      = false;

   RARCH_LOG("[Playlist]: Written to playlist file: %s\n", playlist->config.path);
end:
   intfstream_close(file);
   free(file);
}

void playlist_write_file(playlist_t *playlist)
{
   size_t i, len;
   intfstream_t *file = NULL;
   bool compressed    = false;

   /* Playlist will be written if any of the
    * following are true:
    * > 'modified' flag is set
    * > Current playlist format (old/new) does not
    *   match requested
    * > Current playlist compression status does
    *   not match requested */
   if (!playlist ||
       !(playlist->modified ||
#if defined(HAVE_ZLIB)
        (playlist->compressed != playlist->config.compress) ||
#endif
        (playlist->old_format != playlist->config.old_format)))
      return;

#if defined(HAVE_ZLIB)
   if (playlist->config.compress)
      file = intfstream_open_rzip_file(playlist->config.path,
            RETRO_VFS_FILE_ACCESS_WRITE);
   else
#endif
      file = intfstream_open_file(playlist->config.path,
            RETRO_VFS_FILE_ACCESS_WRITE,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!file)
   {
      RARCH_ERR("Failed to write to playlist file: %s\n", playlist->config.path);
      return;
   }

   /* Get current file compression state */
   compressed = intfstream_is_compressed(file);

#ifdef RARCH_INTERNAL
   if (playlist->config.old_format)
   {
      for (i = 0, len = RBUF_LEN(playlist->entries); i < len; i++)
         intfstream_printf(file, "%s\n%s\n%s\n%s\n%s\n%s\n",
               playlist->entries[i].path      ? playlist->entries[i].path      : "",
               playlist->entries[i].label     ? playlist->entries[i].label     : "",
               playlist->entries[i].core_path ? playlist->entries[i].core_path : "",
               playlist->entries[i].core_name ? playlist->entries[i].core_name : "",
               playlist->entries[i].crc32     ? playlist->entries[i].crc32     : "",
               playlist->entries[i].db_name   ? playlist->entries[i].db_name   : ""
               );

      /* Add metadata lines
       * > We add these at the end of the file to prevent
       *   breakage if the playlist is loaded with an older
       *   version of RetroArch */
      intfstream_printf(
            file,
            "default_core_path = \"%s\"\n"
            "default_core_name = \"%s\"\n"
            "label_display_mode = \"%d\"\n"
            "thumbnail_mode = \"%d|%d\"\n"
            "sort_mode = \"%d\"\n",
            playlist->default_core_path ? playlist->default_core_path : "",
            playlist->default_core_name ? playlist->default_core_name : "",
            playlist->label_display_mode,
            playlist->right_thumbnail_mode, playlist->left_thumbnail_mode,
            playlist->sort_mode);

      playlist->old_format = true;
   }
   else
#endif
   {
      rjsonwriter_t* writer = rjsonwriter_open_stream(file);
      if (!writer)
      {
         RARCH_ERR("Failed to create JSON writer\n");
         goto end;
      }
      /*  When compressing playlists, human readability
       *   is not a factor - can skip all indentation
       *   and new line characters */
      if (compressed)
         rjsonwriter_set_options(writer, RJSONWRITER_OPTION_SKIP_WHITESPACE);

      rjsonwriter_raw(writer, "{", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 2);
      rjsonwriter_add_string(writer, "version");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_add_string(writer, "1.5");
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 2);
      rjsonwriter_add_string(writer, "default_core_path");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_add_string(writer, playlist->default_core_path);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 2);
      rjsonwriter_add_string(writer, "default_core_name");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_add_string(writer, playlist->default_core_name);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      if (!string_is_empty(playlist->base_content_directory))
      {
         rjsonwriter_add_spaces(writer, 2);
         rjsonwriter_add_string(writer, "base_content_directory");
         rjsonwriter_raw(writer, ":", 1);
         rjsonwriter_raw(writer, " ", 1);
         rjsonwriter_add_string(writer, playlist->base_content_directory);
         rjsonwriter_raw(writer, ",", 1);
         rjsonwriter_raw(writer, "\n", 1);
      }

      rjsonwriter_add_spaces(writer, 2);
      rjsonwriter_add_string(writer, "label_display_mode");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_rawf(writer, "%d", (int)playlist->label_display_mode);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 2);
      rjsonwriter_add_string(writer, "right_thumbnail_mode");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_rawf(writer, "%d", (int)playlist->right_thumbnail_mode);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 2);
      rjsonwriter_add_string(writer, "left_thumbnail_mode");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_rawf(writer, "%d", (int)playlist->left_thumbnail_mode);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 2);
      rjsonwriter_add_string(writer, "sort_mode");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_rawf(writer, "%d", (int)playlist->sort_mode);
      rjsonwriter_raw(writer, ",", 1);
      rjsonwriter_raw(writer, "\n", 1);

      if (!string_is_empty(playlist->scan_record.content_dir))
      {
         rjsonwriter_add_spaces(writer, 2);
         rjsonwriter_add_string(writer, "scan_content_dir");
         rjsonwriter_raw(writer, ":", 1);
         rjsonwriter_raw(writer, " ", 1);
         rjsonwriter_add_string(writer, playlist->scan_record.content_dir);
         rjsonwriter_raw(writer, ",", 1);
         rjsonwriter_raw(writer, "\n", 1);

         rjsonwriter_add_spaces(writer, 2);
         rjsonwriter_add_string(writer, "scan_file_exts");
         rjsonwriter_raw(writer, ":", 1);
         rjsonwriter_raw(writer, " ", 1);
         rjsonwriter_add_string(writer, playlist->scan_record.file_exts);
         rjsonwriter_raw(writer, ",", 1);
         rjsonwriter_raw(writer, "\n", 1);

         rjsonwriter_add_spaces(writer, 2);
         rjsonwriter_add_string(writer, "scan_dat_file_path");
         rjsonwriter_raw(writer, ":", 1);
         rjsonwriter_raw(writer, " ", 1);
         rjsonwriter_add_string(writer, playlist->scan_record.dat_file_path);
         rjsonwriter_raw(writer, ",", 1);
         rjsonwriter_raw(writer, "\n", 1);

         rjsonwriter_add_spaces(writer, 2);
         rjsonwriter_add_string(writer, "scan_search_recursively");
         rjsonwriter_raw(writer, ":", 1);
         rjsonwriter_raw(writer, " ", 1);
         {
            bool value = playlist->scan_record.search_recursively;
            rjsonwriter_raw(writer, (value ? "true" : "false"), (value ? 4 : 5));
         }
         rjsonwriter_raw(writer, ",", 1);
         rjsonwriter_raw(writer, "\n", 1);

         rjsonwriter_add_spaces(writer, 2);
         rjsonwriter_add_string(writer, "scan_search_archives");
         rjsonwriter_raw(writer, ":", 1);
         rjsonwriter_raw(writer, " ", 1);
         {
            bool value = playlist->scan_record.search_archives;
            rjsonwriter_raw(writer, (value ? "true" : "false"), (value ? 4 : 5));
         }
         rjsonwriter_raw(writer, ",", 1);
         rjsonwriter_raw(writer, "\n", 1);

         rjsonwriter_add_spaces(writer, 2);
         rjsonwriter_add_string(writer, "scan_filter_dat_content");
         rjsonwriter_raw(writer, ":", 1);
         rjsonwriter_raw(writer, " ", 1);
         {
            bool value = playlist->scan_record.filter_dat_content;
            rjsonwriter_raw(writer, (value ? "true" : "false"), (value ? 4 : 5));
         }
         rjsonwriter_raw(writer, ",", 1);
         rjsonwriter_raw(writer, "\n", 1);
      }

      rjsonwriter_add_spaces(writer, 2);
      rjsonwriter_add_string(writer, "items");
      rjsonwriter_raw(writer, ":", 1);
      rjsonwriter_raw(writer, " ", 1);
      rjsonwriter_raw(writer, "[", 1);
      rjsonwriter_raw(writer, "\n", 1);

      for (i = 0, len = RBUF_LEN(playlist->entries); i < len; i++)
      {
         rjsonwriter_add_spaces(writer, 4);
         rjsonwriter_raw(writer, "{", 1);

         rjsonwriter_raw(writer, "\n", 1);
         rjsonwriter_add_spaces(writer, 6);
         rjsonwriter_add_string(writer, "path");
         rjsonwriter_raw(writer, ":", 1);
         rjsonwriter_raw(writer, " ", 1);
         rjsonwriter_add_string(writer, playlist->entries[i].path);
         rjsonwriter_raw(writer, ",", 1);

         if (playlist->entries[i].entry_slot)
         {
            rjsonwriter_raw(writer, "\n", 1);
            rjsonwriter_add_spaces(writer, 6);
            rjsonwriter_add_string(writer, "entry_slot");
            rjsonwriter_raw(writer, ":", 1);
            rjsonwriter_raw(writer, " ", 1);
            rjsonwriter_rawf(writer, "%d", (int)playlist->entries[i].entry_slot);
            rjsonwriter_raw(writer, ",", 1);
         }

         rjsonwriter_raw(writer, "\n", 1);
         rjsonwriter_add_spaces(writer, 6);
         rjsonwriter_add_string(writer, "label");
         rjsonwriter_raw(writer, ":", 1);
         rjsonwriter_raw(writer, " ", 1);
         rjsonwriter_add_string(writer, playlist->entries[i].label);
         rjsonwriter_raw(writer, ",", 1);

         rjsonwriter_raw(writer, "\n", 1);
         rjsonwriter_add_spaces(writer, 6);
         rjsonwriter_add_string(writer, "core_path");
         rjsonwriter_raw(writer, ":", 1);
         rjsonwriter_raw(writer, " ", 1);
         rjsonwriter_add_string(writer, playlist->entries[i].core_path);
         rjsonwriter_raw(writer, ",", 1);

         rjsonwriter_raw(writer, "\n", 1);
         rjsonwriter_add_spaces(writer, 6);
         rjsonwriter_add_string(writer, "core_name");
         rjsonwriter_raw(writer, ":", 1);
         rjsonwriter_raw(writer, " ", 1);
         rjsonwriter_add_string(writer, playlist->entries[i].core_name);
         rjsonwriter_raw(writer, ",", 1);

         rjsonwriter_raw(writer, "\n", 1);
         rjsonwriter_add_spaces(writer, 6);
         rjsonwriter_add_string(writer, "crc32");
         rjsonwriter_raw(writer, ":", 1);
         rjsonwriter_raw(writer, " ", 1);
         rjsonwriter_add_string(writer, playlist->entries[i].crc32);
         rjsonwriter_raw(writer, ",", 1);

         rjsonwriter_raw(writer, "\n", 1);
         rjsonwriter_add_spaces(writer, 6);
         rjsonwriter_add_string(writer, "db_name");
         rjsonwriter_raw(writer, ":", 1);
         rjsonwriter_raw(writer, " ", 1);
         rjsonwriter_add_string(writer, playlist->entries[i].db_name);

         if (!string_is_empty(playlist->entries[i].subsystem_ident))
         {
            rjsonwriter_raw(writer, ",", 1);
            rjsonwriter_raw(writer, "\n", 1);
            rjsonwriter_add_spaces(writer, 6);
            rjsonwriter_add_string(writer, "subsystem_ident");
            rjsonwriter_raw(writer, ":", 1);
            rjsonwriter_raw(writer, " ", 1);
            rjsonwriter_add_string(writer, playlist->entries[i].subsystem_ident);
         }

         if (!string_is_empty(playlist->entries[i].subsystem_name))
         {
            rjsonwriter_raw(writer, ",", 1);
            rjsonwriter_raw(writer, "\n", 1);
            rjsonwriter_add_spaces(writer, 6);
            rjsonwriter_add_string(writer, "subsystem_name");
            rjsonwriter_raw(writer, ":", 1);
            rjsonwriter_raw(writer, " ", 1);
            rjsonwriter_add_string(writer, playlist->entries[i].subsystem_name);
         }

         if (  playlist->entries[i].subsystem_roms &&
               playlist->entries[i].subsystem_roms->size > 0)
         {
            unsigned j;

            rjsonwriter_raw(writer, ",", 1);
            rjsonwriter_raw(writer, "\n", 1);
            rjsonwriter_add_spaces(writer, 6);
            rjsonwriter_add_string(writer, "subsystem_roms");
            rjsonwriter_raw(writer, ":", 1);
            rjsonwriter_raw(writer, " ", 1);
            rjsonwriter_raw(writer, "[", 1);
            rjsonwriter_raw(writer, "\n", 1);

            for (j = 0; j < playlist->entries[i].subsystem_roms->size; j++)
            {
               const struct string_list *roms = playlist->entries[i].subsystem_roms;
               rjsonwriter_add_spaces(writer, 8);
               rjsonwriter_add_string(writer,
                     !string_is_empty(roms->elems[j].data)
                     ? roms->elems[j].data
                     : "");

               if (j < playlist->entries[i].subsystem_roms->size - 1)
               {
                  rjsonwriter_raw(writer, ",", 1);
                  rjsonwriter_raw(writer, "\n", 1);
               }
            }

            rjsonwriter_raw(writer, "\n", 1);
            rjsonwriter_add_spaces(writer, 6);
            rjsonwriter_raw(writer, "]", 1);
         }

         rjsonwriter_raw(writer, "\n", 1);

         rjsonwriter_add_spaces(writer, 4);
         rjsonwriter_raw(writer, "}", 1);

         if (i < len - 1)
            rjsonwriter_raw(writer, ",", 1);

         rjsonwriter_raw(writer, "\n", 1);
      }

      rjsonwriter_add_spaces(writer, 2);
      rjsonwriter_raw(writer, "]", 1);
      rjsonwriter_raw(writer, "\n", 1);
      rjsonwriter_raw(writer, "}", 1);
      rjsonwriter_raw(writer, "\n", 1);

      if (!rjsonwriter_free(writer))
      {
         RARCH_ERR("Failed to write to playlist file: %s\n", playlist->config.path);
      }

      playlist->old_format = false;
   }

   playlist->modified   = false;
   playlist->compressed = compressed;

   RARCH_LOG("[Playlist]: Written to playlist file: %s\n", playlist->config.path);
end:
   intfstream_close(file);
   free(file);
}

/**
 * playlist_free:
 * @playlist            : Playlist handle.
 *
 * Frees playlist handle.
 */
void playlist_free(playlist_t *playlist)
{
   size_t i, len;

   if (!playlist)
      return;

   if (playlist->default_core_path)
      free(playlist->default_core_path);
   playlist->default_core_path = NULL;

   if (playlist->default_core_name)
      free(playlist->default_core_name);
   playlist->default_core_name = NULL;

   if (playlist->base_content_directory)
      free(playlist->base_content_directory);
   playlist->base_content_directory = NULL;

   if (playlist->scan_record.content_dir)
      free(playlist->scan_record.content_dir);
   playlist->scan_record.content_dir = NULL;

   if (playlist->scan_record.file_exts)
      free(playlist->scan_record.file_exts);
   playlist->scan_record.file_exts = NULL;

   if (playlist->scan_record.dat_file_path)
      free(playlist->scan_record.dat_file_path);
   playlist->scan_record.dat_file_path = NULL;

   if (playlist->entries)
   {
      for (i = 0, len = RBUF_LEN(playlist->entries); i < len; i++)
      {
         struct playlist_entry *entry = &playlist->entries[i];

         if (entry)
            playlist_free_entry(entry);
      }

      RBUF_FREE(playlist->entries);
   }

   free(playlist);
}

/**
 * playlist_clear:
 * @playlist        	   : Playlist handle.
 *
 * Clears all playlist entries in playlist.
 **/
void playlist_clear(playlist_t *playlist)
{
   size_t i, len;
   if (!playlist)
      return;

   for (i = 0, len = RBUF_LEN(playlist->entries); i < len; i++)
   {
      struct playlist_entry *entry = &playlist->entries[i];

      if (entry)
         playlist_free_entry(entry);
   }
   RBUF_CLEAR(playlist->entries);
}

/**
 * playlist_size:
 * @playlist        	   : Playlist handle.
 *
 * Gets size of playlist.
 * Returns: size of playlist.
 **/
size_t playlist_size(playlist_t *playlist)
{
   if (!playlist)
      return 0;
   return RBUF_LEN(playlist->entries);
}

/**
 * playlist_capacity:
 * @playlist        	   : Playlist handle.
 *
 * Gets maximum capacity of playlist.
 * Returns: maximum capacity of playlist.
 **/
size_t playlist_capacity(playlist_t *playlist)
{
   if (!playlist)
      return 0;
   return playlist->config.capacity;
}

static bool JSONStartArrayHandler(void *context)
{
   JSONContext *pCtx = (JSONContext *)context;

   pCtx->array_depth++;

   return true;
}

static bool JSONEndArrayHandler(void *context)
{
   JSONContext *pCtx = (JSONContext *)context;

   retro_assert(pCtx->array_depth > 0);

   pCtx->array_depth--;

   if (pCtx->in_items && pCtx->array_depth == 0 && pCtx->object_depth <= 1)
      pCtx->in_items = false;
   else if (pCtx->in_subsystem_roms && pCtx->array_depth <= 1 && pCtx->object_depth <= 2)
      pCtx->in_subsystem_roms = false;

   return true;
}

static bool JSONStartObjectHandler(void *context)
{
   JSONContext *pCtx = (JSONContext *)context;

   pCtx->object_depth++;

   if (pCtx->in_items && pCtx->object_depth == 2)
   {
      if ((pCtx->array_depth == 1) && !pCtx->capacity_exceeded)
      {
         size_t len = RBUF_LEN(pCtx->playlist->entries);
         if (len < pCtx->playlist->config.capacity)
         {
            /* Allocate memory to fit one more item but don't resize the
             * buffer just yet, wait until JSONEndObjectHandler for that */
            if (!RBUF_TRYFIT(pCtx->playlist->entries, len + 1))
            {
               pCtx->out_of_memory     = true;
               return false;
            }
            pCtx->current_entry = &pCtx->playlist->entries[len];
            memset(pCtx->current_entry, 0, sizeof(*pCtx->current_entry));
         }
         else
         {
            /* Hit max item limit.
             * Note: We can't just abort here, since there may
             * be more metadata to read at the end of the file... */
            RARCH_WARN("JSON file contains more entries than current playlist capacity. Excess entries will be discarded.\n");
            pCtx->capacity_exceeded  = true;
            pCtx->current_entry      = NULL;
            /* In addition, since we are discarding excess entries,
             * the playlist must be flagged as being modified
             * (i.e. the playlist is not the same as when it was
             * last saved to disk...) */
            pCtx->playlist->modified = true;
         }
      }
   }

   return true;
}

static bool JSONEndObjectHandler(void *context)
{
   JSONContext *pCtx = (JSONContext *)context;

   if (pCtx->in_items && pCtx->object_depth == 2)
   {
      if ((pCtx->array_depth == 1) && !pCtx->capacity_exceeded)
         RBUF_RESIZE(pCtx->playlist->entries,
               RBUF_LEN(pCtx->playlist->entries) + 1);
   }

   retro_assert(pCtx->object_depth > 0);

   pCtx->object_depth--;

   return true;
}

static bool JSONStringHandler(void *context, const char *pValue, size_t length)
{
   JSONContext *pCtx = (JSONContext *)context;

   if (pCtx->in_items && pCtx->in_subsystem_roms && pCtx->object_depth == 2 && pCtx->array_depth == 2)
   {
      if (length && !string_is_empty(pValue))
      {
         union string_list_elem_attr attr = {0};

         if (!pCtx->current_entry->subsystem_roms)
            pCtx->current_entry->subsystem_roms = string_list_new();

         string_list_append(pCtx->current_entry->subsystem_roms, pValue, attr);
      }
   }
   else if (pCtx->in_items && pCtx->object_depth == 2)
   {
      if (pCtx->array_depth == 1)
      {
         if (pCtx->current_string_val && length && !string_is_empty(pValue))
         {
            if (*pCtx->current_string_val)
                free(*pCtx->current_string_val);
             *pCtx->current_string_val = strdup(pValue);
         }
      }
   }
   else if (pCtx->object_depth == 1)
   {
      if (pCtx->array_depth == 0)
      {
         if (pCtx->current_string_val && length && !string_is_empty(pValue))
         {
            /* handle any top-level playlist metadata here */
            if (*pCtx->current_string_val)
                free(*pCtx->current_string_val);
            *pCtx->current_string_val = strdup(pValue);
         }
      }
   }

   pCtx->current_string_val = NULL;

   return true;
}

static bool JSONNumberHandler(void *context, const char *pValue, size_t length)
{
   JSONContext *pCtx = (JSONContext *)context;

   if (pCtx->in_items && pCtx->object_depth == 2)
   {
      if (pCtx->array_depth == 1 && length && !string_is_empty(pValue))
      {
         if (pCtx->current_entry_uint_val)
            *pCtx->current_entry_uint_val = (unsigned)strtoul(pValue, NULL, 10);
      }
   }
   else if (pCtx->object_depth == 1)
   {
      if (pCtx->array_depth == 0)
      {
         if (length && !string_is_empty(pValue))
         {
            /* handle any top-level playlist metadata here */
            if (pCtx->current_meta_label_display_mode_val)
               *pCtx->current_meta_label_display_mode_val = (enum playlist_label_display_mode)strtoul(pValue, NULL, 10);
            else if (pCtx->current_meta_thumbnail_mode_val)
               *pCtx->current_meta_thumbnail_mode_val = (enum playlist_thumbnail_mode)strtoul(pValue, NULL, 10);
            else if (pCtx->current_meta_sort_mode_val)
               *pCtx->current_meta_sort_mode_val = (enum playlist_sort_mode)strtoul(pValue, NULL, 10);
         }
      }
   }

   pCtx->current_entry_uint_val              = NULL;
   pCtx->current_meta_label_display_mode_val = NULL;
   pCtx->current_meta_thumbnail_mode_val     = NULL;
   pCtx->current_meta_sort_mode_val          = NULL;

   return true;
}

static bool JSONBoolHandler(void *context, bool value)
{
   JSONContext *pCtx = (JSONContext *)context;

   if (!pCtx->in_items &&
       (pCtx->object_depth == 1) &&
       (pCtx->array_depth == 0) &&
       pCtx->current_meta_bool_val)
      *pCtx->current_meta_bool_val = value;

   pCtx->current_meta_bool_val = NULL;

   return true;
}

static bool JSONObjectMemberHandler(void *context, const char *pValue, size_t length)
{
   JSONContext *pCtx = (JSONContext *)context;

   if (pCtx->in_items && pCtx->object_depth == 2)
   {
      if (pCtx->array_depth == 1)
      {
         if (pCtx->current_string_val)
         {
            /* something went wrong */
            return false;
         }

         if (length && !pCtx->capacity_exceeded)
         {
            pCtx->current_string_val     = NULL;
            pCtx->current_entry_uint_val = NULL;
            pCtx->in_subsystem_roms      = false;
            switch (pValue[0])
            {
               case 'c':
                     if (string_is_equal(pValue, "core_name"))
                        pCtx->current_string_val = &pCtx->current_entry->core_name;
                     else if (string_is_equal(pValue, "core_path"))
                        pCtx->current_string_val = &pCtx->current_entry->core_path;
                     else if (string_is_equal(pValue, "crc32"))
                        pCtx->current_string_val = &pCtx->current_entry->crc32;
                     break;
               case 'd':
                     if (string_is_equal(pValue, "db_name"))
                        pCtx->current_string_val = &pCtx->current_entry->db_name;
                     break;
               case 'e':
                     if (string_is_equal(pValue, "entry_slot"))
                        pCtx->current_entry_uint_val = &pCtx->current_entry->entry_slot;
                     break;
               case 'l':
                     if (string_is_equal(pValue, "label"))
                        pCtx->current_string_val = &pCtx->current_entry->label;
                     else if (string_is_equal(pValue, "last_played_day"))
                        pCtx->current_entry_uint_val = &pCtx->current_entry->last_played_day;
                     else if (string_is_equal(pValue, "last_played_hour"))
                        pCtx->current_entry_uint_val = &pCtx->current_entry->last_played_hour;
                     else if (string_is_equal(pValue, "last_played_minute"))
                        pCtx->current_entry_uint_val = &pCtx->current_entry->last_played_minute;
                     else if (string_is_equal(pValue, "last_played_month"))
                        pCtx->current_entry_uint_val = &pCtx->current_entry->last_played_month;
                     else if (string_is_equal(pValue, "last_played_second"))
                        pCtx->current_entry_uint_val = &pCtx->current_entry->last_played_second;
                     else if (string_is_equal(pValue, "last_played_year"))
                        pCtx->current_entry_uint_val = &pCtx->current_entry->last_played_year;
                     break;
               case 'p':
                     if (string_is_equal(pValue, "path"))
                        pCtx->current_string_val = &pCtx->current_entry->path;
                     break;
               case 'r':
                     if (string_is_equal(pValue, "runtime_hours"))
                        pCtx->current_entry_uint_val = &pCtx->current_entry->runtime_hours;
                     else if (string_is_equal(pValue, "runtime_minutes"))
                        pCtx->current_entry_uint_val = &pCtx->current_entry->runtime_minutes;
                     else if (string_is_equal(pValue, "runtime_seconds"))
                        pCtx->current_entry_uint_val = &pCtx->current_entry->runtime_seconds;
                     break;
               case 's':
                     if (string_is_equal(pValue, "subsystem_ident"))
                        pCtx->current_string_val = &pCtx->current_entry->subsystem_ident;
                     else if (string_is_equal(pValue, "subsystem_name"))
                        pCtx->current_string_val = &pCtx->current_entry->subsystem_name;
                     else if (string_is_equal(pValue, "subsystem_roms"))
                        pCtx->in_subsystem_roms = true;
                     break;
            }
         }
      }
   }
   else if (pCtx->object_depth == 1 && pCtx->array_depth == 0 && length)
   {
      pCtx->current_string_val                  = NULL;
      pCtx->current_meta_label_display_mode_val = NULL;
      pCtx->current_meta_thumbnail_mode_val     = NULL;
      pCtx->current_meta_sort_mode_val          = NULL;
      pCtx->current_meta_bool_val               = NULL;
      pCtx->in_items                            = false;

      switch (pValue[0])
      {
         case 'b':
            if (string_is_equal(pValue, "base_content_directory"))
               pCtx->current_string_val = &pCtx->playlist->base_content_directory;
            break;
         case 'd':
            if (string_is_equal(pValue,      "default_core_path"))
               pCtx->current_string_val = &pCtx->playlist->default_core_path;
            else if (string_is_equal(pValue, "default_core_name"))
               pCtx->current_string_val = &pCtx->playlist->default_core_name;
            break;
         case 'i':
            if (string_is_equal(pValue, "items"))
               pCtx->in_items = true;
            break;
         case 'l':
            if (string_is_equal(pValue,      "label_display_mode"))
               pCtx->current_meta_label_display_mode_val = &pCtx->playlist->label_display_mode;
            else if (string_is_equal(pValue, "left_thumbnail_mode"))
               pCtx->current_meta_thumbnail_mode_val     = &pCtx->playlist->left_thumbnail_mode;
            break;
         case 'r':
            if (string_is_equal(pValue, "right_thumbnail_mode"))
               pCtx->current_meta_thumbnail_mode_val = &pCtx->playlist->right_thumbnail_mode;
            break;
         case 's':
            if (string_is_equal(pValue,      "scan_content_dir"))
               pCtx->current_string_val         = &pCtx->playlist->scan_record.content_dir;
            else if (string_is_equal(pValue, "scan_file_exts"))
               pCtx->current_string_val         = &pCtx->playlist->scan_record.file_exts;
            else if (string_is_equal(pValue, "scan_dat_file_path"))
               pCtx->current_string_val         = &pCtx->playlist->scan_record.dat_file_path;
            else if (string_is_equal(pValue, "scan_search_recursively"))
               pCtx->current_meta_bool_val      = &pCtx->playlist->scan_record.search_recursively;
            else if (string_is_equal(pValue, "scan_search_archives"))
               pCtx->current_meta_bool_val      = &pCtx->playlist->scan_record.search_archives;
            else if (string_is_equal(pValue, "scan_filter_dat_content"))
               pCtx->current_meta_bool_val      = &pCtx->playlist->scan_record.filter_dat_content;
            else if (string_is_equal(pValue, "sort_mode"))
               pCtx->current_meta_sort_mode_val = &pCtx->playlist->sort_mode;
            break;
      }
   }

   return true;
}

static void get_old_format_metadata_value(
      char *metadata_line, char *value, size_t len)
{
   char *end   = NULL;
   char *start = strchr(metadata_line, '\"');

   if (!start)
      return;

   start++;
   end         = strchr(start, '\"');

   if (!end)
      return;

   *end        = '\0';
   strlcpy(value, start, len);
}

static bool playlist_read_file(playlist_t *playlist)
{
   unsigned i;
   int test_char;
   bool res = true;

#if defined(HAVE_ZLIB)
      /* Always use RZIP interface when reading playlists
       * > this will automatically handle uncompressed
       *   data */
   intfstream_t *file   = intfstream_open_rzip_file(
         playlist->config.path,
         RETRO_VFS_FILE_ACCESS_READ);
#else
   intfstream_t *file   = intfstream_open_file(
         playlist->config.path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
#endif

   /* If playlist file does not exist,
    * create an empty playlist instead */
   if (!file)
      return true;

   playlist->compressed = intfstream_is_compressed(file);

   /* Detect format of playlist
    * > Read file until we find the first printable
    *   non-whitespace ASCII character */
   do
   {
	   /* Read error or EOF (end of file) */
      if ((test_char = intfstream_getc(file)) == EOF)
         goto end;
   }while (!isgraph(test_char) || test_char > 0x7F);

   if (test_char == '{')
   {
      /* New playlist format detected */
#if 0
      RARCH_LOG("[Playlist]: New playlist format detected.\n");
#endif
      playlist->old_format = false;
   }
   else
   {
      /* old playlist format detected */
#if 0
      RARCH_LOG("[Playlist]: Old playlist format detected.\n");
#endif
      playlist->old_format = true;
   }

   /* Reset file to start */
   intfstream_rewind(file);

   if (!playlist->old_format)
   {
      rjson_t* parser;
      JSONContext context = {0};
      context.playlist    = playlist;

      parser = rjson_open_stream(file);
      if (!parser)
      {
         RARCH_ERR("Failed to create JSON parser\n");
         goto end;
      }

      rjson_set_options(parser,
              RJSON_OPTION_ALLOW_UTF8BOM
            | RJSON_OPTION_ALLOW_COMMENTS
            | RJSON_OPTION_ALLOW_UNESCAPED_CONTROL_CHARACTERS
            | RJSON_OPTION_REPLACE_INVALID_ENCODING);

      if (rjson_parse(parser, &context,
            JSONObjectMemberHandler,
            JSONStringHandler,
            JSONNumberHandler,
            JSONStartObjectHandler,
            JSONEndObjectHandler,
            JSONStartArrayHandler,
            JSONEndArrayHandler,
            JSONBoolHandler,
            NULL) /* Unused null handler */
            != RJSON_DONE)
      {
         if (context.out_of_memory)
         {
            RARCH_WARN("Ran out of memory while parsing JSON playlist\n");
            res = false;
         }
         else
         {
            RARCH_WARN("Error parsing chunk:\n---snip---\n%.*s\n---snip---\n",
                  rjson_get_source_context_len(parser),
                  rjson_get_source_context_buf(parser));
            RARCH_WARN("Error: Invalid JSON at line %d, column %d - %s.\n",
                  (int)rjson_get_source_line(parser),
                  (int)rjson_get_source_column(parser),
                  (*rjson_get_error(parser) ? rjson_get_error(parser) : "format error"));
         }
      }
      rjson_free(parser);
   }
   else
   {
      size_t len = RBUF_LEN(playlist->entries);
      char line_buf[PLAYLIST_ENTRIES][PATH_MAX_LENGTH] = {{0}};

      /* Unnecessary, but harmless */
      for (i = 0; i < PLAYLIST_ENTRIES; i++)
         line_buf[i][0] = '\0';

      /* Read playlist entries */
      while (len < playlist->config.capacity)
      {
         size_t i;
         size_t lines_read = 0;

         /* Attempt to read the next 'PLAYLIST_ENTRIES'
          * lines from the file */
         for (i = 0; i < PLAYLIST_ENTRIES; i++)
         {
            *line_buf[i] = '\0';

            if (intfstream_gets(file, line_buf[i], sizeof(line_buf[i])))
            {
               /* Ensure line is NUL terminated, regardless of
                * Windows or Unix line endings */
               string_replace_all_chars(line_buf[i], '\r', '\0');
               string_replace_all_chars(line_buf[i], '\n', '\0');

               lines_read++;
            }
            else
               break;
         }

         /* If a 'full set' of lines were read, then this
          * is a valid playlist entry */
         if (lines_read >= PLAYLIST_ENTRIES)
         {
            struct playlist_entry* entry;

            if (!RBUF_TRYFIT(playlist->entries, len + 1))
            {
               res = false; /* out of memory */
               goto end;
            }
            RBUF_RESIZE(playlist->entries, len + 1);
            entry = &playlist->entries[len++];

            memset(entry, 0, sizeof(*entry));

            /* path */
            if (!string_is_empty(line_buf[0]))
               entry->path      = strdup(line_buf[0]);

            /* label */
            if (!string_is_empty(line_buf[1]))
               entry->label     = strdup(line_buf[1]);

            /* core_path */
            if (!string_is_empty(line_buf[2]))
               entry->core_path = strdup(line_buf[2]);

            /* core_name */
            if (!string_is_empty(line_buf[3]))
               entry->core_name = strdup(line_buf[3]);

            /* crc32 */
            if (!string_is_empty(line_buf[4]))
               entry->crc32     = strdup(line_buf[4]);

            /* db_name */
            if (!string_is_empty(line_buf[5]))
               entry->db_name   = strdup(line_buf[5]);
         }
         /* If fewer than 'PLAYLIST_ENTRIES' lines were
          * read, then this is metadata */
         else
         {
            char default_core_path[PATH_MAX_LENGTH];
            char default_core_name[PATH_MAX_LENGTH];

            default_core_path[0] = '\0';
            default_core_name[0] = '\0';

            /* Get default_core_path */
            if (lines_read < 1)
               break;

            if (strncmp("default_core_path",
                     line_buf[0],
                     STRLEN_CONST("default_core_path")) == 0)
               get_old_format_metadata_value(
                     line_buf[0], default_core_path, sizeof(default_core_path));

            /* Get default_core_name */
            if (lines_read < 2)
               break;

            if (strncmp("default_core_name",
                     line_buf[1],
                     STRLEN_CONST("default_core_name")) == 0)
               get_old_format_metadata_value(
                     line_buf[1], default_core_name, sizeof(default_core_name));

            /* > Populate default core path/name, if required
             *   (if one is empty, the other should be ignored) */
            if (!string_is_empty(default_core_path) &&
                !string_is_empty(default_core_name))
            {
               playlist->default_core_path = strdup(default_core_path);
               playlist->default_core_name = strdup(default_core_name);
            }

            /* Get label_display_mode */
            if (lines_read < 3)
               break;

            if (strncmp("label_display_mode",
                     line_buf[2],
                     STRLEN_CONST("label_display_mode")) == 0)
            {
               unsigned display_mode;
               char display_mode_str[4] = {0};

               get_old_format_metadata_value(
                     line_buf[2], display_mode_str, sizeof(display_mode_str));

               display_mode = string_to_unsigned(display_mode_str);

               if (display_mode <= LABEL_DISPLAY_MODE_KEEP_REGION_AND_DISC_INDEX)
                  playlist->label_display_mode = (enum playlist_label_display_mode)display_mode;
            }

            /* Get thumbnail modes */
            if (lines_read < 4)
               break;

            if (strncmp("thumbnail_mode",
                     line_buf[3],
                     STRLEN_CONST("thumbnail_mode")) == 0)
            {
               char thumbnail_mode_str[8]          = {0};
               struct string_list thumbnail_modes  = {0};

               get_old_format_metadata_value(
                     line_buf[3], thumbnail_mode_str,
                     sizeof(thumbnail_mode_str));
               string_list_initialize(&thumbnail_modes);
               if (string_split_noalloc(&thumbnail_modes,
                        thumbnail_mode_str, "|"))
               {
                  if (thumbnail_modes.size == 2)
                  {
                     unsigned thumbnail_mode;

                     /* Right thumbnail mode */
                     thumbnail_mode = string_to_unsigned(
                           thumbnail_modes.elems[0].data);
                     if (thumbnail_mode <= PLAYLIST_THUMBNAIL_MODE_BOXARTS)
                        playlist->right_thumbnail_mode = (enum playlist_thumbnail_mode)thumbnail_mode;

                     /* Left thumbnail mode */
                     thumbnail_mode = string_to_unsigned(
                           thumbnail_modes.elems[1].data);
                     if (thumbnail_mode <= PLAYLIST_THUMBNAIL_MODE_BOXARTS)
                        playlist->left_thumbnail_mode = (enum playlist_thumbnail_mode)thumbnail_mode;
                  }
               }
               string_list_deinitialize(&thumbnail_modes);
            }

            /* Get sort_mode */
            if (lines_read < 5)
               break;

            if (strncmp("sort_mode",
                     line_buf[4],
                     STRLEN_CONST("sort_mode")) == 0)
            {
               unsigned sort_mode;
               char sort_mode_str[4] = {0};

               get_old_format_metadata_value(
                     line_buf[4], sort_mode_str, sizeof(sort_mode_str));

               sort_mode = string_to_unsigned(sort_mode_str);

               if (sort_mode <= PLAYLIST_SORT_MODE_OFF)
                  playlist->sort_mode = (enum playlist_sort_mode)sort_mode;
            }

            /* All metadata parsed -> end of file */
            break;
         }
      }
   }

end:
   intfstream_close(file);
   free(file);
   return res;
}

void playlist_free_cached(void)
{
   if (playlist_cached && !playlist_cached->cached_external)
      playlist_free(playlist_cached);
   playlist_cached = NULL;
}

playlist_t *playlist_get_cached(void)
{
   if (playlist_cached)
      return playlist_cached;
   return NULL;
}

bool playlist_init_cached(const playlist_config_t *config)
{
   playlist_t *playlist = playlist_init(config);
   if (!playlist)
      return false;

   /* If playlist format/compression state
    * does not match requested settings, update
    * file on disk immediately */
   if (
#if defined(HAVE_ZLIB)
       (playlist->compressed != playlist->config.compress) ||
#endif
       (playlist->old_format != playlist->config.old_format))
      playlist_write_file(playlist);

   playlist_cached      = playlist;
   return true;
}

/**
 * playlist_init:
 * @config            	: Playlist configuration object.
 *
 * Creates and initializes a playlist.
 *
 * Returns: handle to new playlist if successful, otherwise NULL
 **/
playlist_t *playlist_init(const playlist_config_t *config)
{
   playlist_t           *playlist = (playlist_t*)malloc(sizeof(*playlist));
   if (!playlist)
      goto error;

   /* Set initial values */
   playlist->modified               = false;
   playlist->old_format             = false;
   playlist->compressed             = false;
   playlist->cached_external        = false;
   playlist->default_core_name      = NULL;
   playlist->default_core_path      = NULL;
   playlist->base_content_directory = NULL;
   playlist->entries                = NULL;
   playlist->label_display_mode     = LABEL_DISPLAY_MODE_DEFAULT;
   playlist->right_thumbnail_mode   = PLAYLIST_THUMBNAIL_MODE_DEFAULT;
   playlist->left_thumbnail_mode    = PLAYLIST_THUMBNAIL_MODE_DEFAULT;
   playlist->sort_mode              = PLAYLIST_SORT_MODE_DEFAULT;

   playlist->scan_record.search_recursively = false;
   playlist->scan_record.search_archives    = false;
   playlist->scan_record.filter_dat_content = false;
   playlist->scan_record.content_dir        = NULL;
   playlist->scan_record.file_exts          = NULL;
   playlist->scan_record.dat_file_path      = NULL;

   /* Cache configuration parameters */
   if (!playlist_config_copy(config, &playlist->config))
      goto error;

   /* Attempt to read any existing playlist file */
   if (!playlist_read_file(playlist))
      goto error;

   /* Try auto-fixing paths if enabled, and playlist
    * base content directory is different */
   if (config->autofix_paths &&
       !string_is_equal(playlist->base_content_directory,
            config->base_content_directory))
   {
      if (!string_is_empty(playlist->base_content_directory))
      {
         size_t i, j, len;
         char tmp_entry_path[PATH_MAX_LENGTH];

         for (i = 0, len = RBUF_LEN(playlist->entries); i < len; i++)
         {
            struct playlist_entry* entry = &playlist->entries[i];

            if (!entry || string_is_empty(entry->path))
               continue;

            /* Fix entry path */
            tmp_entry_path[0] = '\0';
            path_replace_base_path_and_convert_to_local_file_system(
                  tmp_entry_path, entry->path,
                  playlist->base_content_directory, playlist->config.base_content_directory,
                  sizeof(tmp_entry_path));

            free(entry->path);
            entry->path = strdup(tmp_entry_path);

            /* Fix subsystem roms paths*/
            if (entry->subsystem_roms && (entry->subsystem_roms->size > 0))
            {
               struct string_list* subsystem_roms_new_paths = string_list_new();
               union string_list_elem_attr attributes = { 0 };

               if (!subsystem_roms_new_paths)
                  goto error;

               for (j = 0; j < entry->subsystem_roms->size; j++)
               {
                  const char* subsystem_rom_path = entry->subsystem_roms->elems[j].data;

                  if (string_is_empty(subsystem_rom_path))
                     continue;

                  tmp_entry_path[0] = '\0';
                  path_replace_base_path_and_convert_to_local_file_system(
                        tmp_entry_path, subsystem_rom_path,
                        playlist->base_content_directory, playlist->config.base_content_directory,
                        sizeof(tmp_entry_path));
                  string_list_append(subsystem_roms_new_paths, tmp_entry_path, attributes);
               }

               string_list_free(entry->subsystem_roms);
               entry->subsystem_roms = subsystem_roms_new_paths;
            }
         }

         /* Fix scan record content directory */
         if (!string_is_empty(playlist->scan_record.content_dir))
         {
            tmp_entry_path[0] = '\0';
            path_replace_base_path_and_convert_to_local_file_system(
                  tmp_entry_path, playlist->scan_record.content_dir,
                  playlist->base_content_directory, playlist->config.base_content_directory,
                  sizeof(tmp_entry_path));

            free(playlist->scan_record.content_dir);
            playlist->scan_record.content_dir = strdup(tmp_entry_path);
         }

         /* Fix scan record arcade DAT file */
         if (!string_is_empty(playlist->scan_record.dat_file_path))
         {
            tmp_entry_path[0] = '\0';
            path_replace_base_path_and_convert_to_local_file_system(
                  tmp_entry_path, playlist->scan_record.dat_file_path,
                  playlist->base_content_directory, playlist->config.base_content_directory,
                  sizeof(tmp_entry_path));

            free(playlist->scan_record.dat_file_path);
            playlist->scan_record.dat_file_path = strdup(tmp_entry_path);
         }
      }

      /* Update playlist base content directory*/
      if (playlist->base_content_directory)
         free(playlist->base_content_directory);
      playlist->base_content_directory = strdup(playlist->config.base_content_directory);

      /* Save playlist */
      playlist->modified = true;
      playlist_write_file(playlist);
   }

   return playlist;

error:
   playlist_free(playlist);
   return NULL;
}

static int playlist_qsort_func(const struct playlist_entry *a,
      const struct playlist_entry *b)
{
   char *a_str            = NULL;
   char *b_str            = NULL;
   char *a_fallback_label = NULL;
   char *b_fallback_label = NULL;
   int ret                = 0;

   if (!a || !b)
      goto end;

   a_str                  = a->label;
   b_str                  = b->label;

   /* It is quite possible for playlist labels
    * to be blank. If that is the case, have to use
    * filename as a fallback (this is slow, but we
    * have no other option...) */
   if (string_is_empty(a_str))
   {
      if (!(a_fallback_label = (char*)calloc(PATH_MAX_LENGTH, sizeof(char))))
         goto end;

      if (!string_is_empty(a->path))
         fill_pathname(a_fallback_label,
               path_basename_nocompression(a->path),
               "", PATH_MAX_LENGTH * sizeof(char));
      /* If filename is also empty, use core name
       * instead -> this matches the behaviour of
       * menu_displaylist_parse_playlist() */
      else if (!string_is_empty(a->core_name))
         strlcpy(a_fallback_label, a->core_name, PATH_MAX_LENGTH * sizeof(char));

      /* If both filename and core name are empty,
       * then have to compare an empty string
       * -> again, this is to match the behaviour of
       * menu_displaylist_parse_playlist() */

      a_str = a_fallback_label;
   }

   if (string_is_empty(b_str))
   {
      if (!(b_fallback_label = (char*)calloc(PATH_MAX_LENGTH, sizeof(char))))
         goto end;

      if (!string_is_empty(b->path))
         fill_pathname(b_fallback_label,
               path_basename_nocompression(b->path), "",
               PATH_MAX_LENGTH * sizeof(char));
      else if (!string_is_empty(b->core_name))
         strlcpy(b_fallback_label, b->core_name, PATH_MAX_LENGTH * sizeof(char));

      b_str = b_fallback_label;
   }

   ret = strcasecmp(a_str, b_str);

end:

   a_str = NULL;
   b_str = NULL;

   if (a_fallback_label)
   {
      free(a_fallback_label);
      a_fallback_label = NULL;
   }

   if (b_fallback_label)
   {
      free(b_fallback_label);
      b_fallback_label = NULL;
   }

   return ret;
}

void playlist_qsort(playlist_t *playlist)
{
   /* Avoid inadvertent sorting if 'sort mode'
    * has been set explicitly to PLAYLIST_SORT_MODE_OFF */
   if (!playlist ||
       (playlist->sort_mode == PLAYLIST_SORT_MODE_OFF) ||
       !playlist->entries)
      return;

   qsort(playlist->entries, RBUF_LEN(playlist->entries),
         sizeof(struct playlist_entry),
         (int (*)(const void *, const void *))playlist_qsort_func);
}

void command_playlist_push_write(
      playlist_t *playlist,
      const struct playlist_entry *entry)
{
   if (!playlist)
      return;

   if (playlist_push(playlist, entry))
      playlist_write_file(playlist);
}

void command_playlist_update_write(
      playlist_t *plist,
      size_t idx,
      const struct playlist_entry *entry)
{
   playlist_t *playlist = plist ? plist : playlist_get_cached();

   if (!playlist)
      return;

   playlist_update(
         playlist,
         idx,
         entry);

   playlist_write_file(playlist);
}

bool playlist_index_is_valid(playlist_t *playlist, size_t idx,
      const char *path, const char *core_path)
{
   if (!playlist)
      return false;

   if (idx >= RBUF_LEN(playlist->entries))
      return false;

   return string_is_equal(playlist->entries[idx].path, path) &&
          string_is_equal(path_basename_nocompression(playlist->entries[idx].core_path), path_basename_nocompression(core_path));
}

bool playlist_entries_are_equal(
      const struct playlist_entry *entry_a,
      const struct playlist_entry *entry_b,
      const playlist_config_t *config)
{
   char real_path_a[PATH_MAX_LENGTH];
   char real_core_path_a[PATH_MAX_LENGTH];

   /* Sanity check */
   if (!entry_a || !entry_b || !config)
      return false;

   if (string_is_empty(entry_a->path) &&
       string_is_empty(entry_a->core_path) &&
       string_is_empty(entry_b->path) &&
       string_is_empty(entry_b->core_path))
      return true;

   /* Check content paths */
   if (!string_is_empty(entry_a->path))
   {
      strlcpy(real_path_a, entry_a->path, sizeof(real_path_a));
      path_resolve_realpath(real_path_a, sizeof(real_path_a), true);
   }
   else
      real_path_a[0]      = '\0';

   if (!playlist_path_equal(
         real_path_a, entry_b->path, config))
      return false;

   /* Check core paths */
   if (!string_is_empty(entry_a->core_path))
   {
      strlcpy(real_core_path_a, entry_a->core_path, sizeof(real_core_path_a));
      if (!string_is_equal(real_core_path_a, FILE_PATH_DETECT) &&
          !string_is_equal(real_core_path_a, FILE_PATH_BUILTIN))
         playlist_resolve_path(PLAYLIST_SAVE, true,
               real_core_path_a, sizeof(real_core_path_a));
   }
   else
      real_core_path_a[0] = '\0';

   return playlist_core_path_equal(real_core_path_a, entry_b->core_path, config);
}

/* Returns true if entries at specified indices
 * of specified playlist have identical content
 * and core paths */
bool playlist_index_entries_are_equal(
      playlist_t *playlist, size_t idx_a, size_t idx_b)
{
   struct playlist_entry *entry_a = NULL;
   struct playlist_entry *entry_b = NULL;
   size_t len;

   if (!playlist)
      return false;

   len = RBUF_LEN(playlist->entries);

   if ((idx_a >= len) || (idx_b >= len))
      return false;

   /* Fetch entries */
   entry_a = &playlist->entries[idx_a];
   entry_b = &playlist->entries[idx_b];

   if (!entry_a || !entry_b)
      return false;

   /* Initialise path ID for entry A, if required
    * (entry B will be handled inside
    * playlist_path_matches_entry()) */
   if (!entry_a->path_id)
      entry_a->path_id = playlist_path_id_init(entry_a->path);

   return playlist_path_matches_entry(
         entry_a->path_id, entry_b, &playlist->config);
}

void playlist_get_crc32(playlist_t *playlist, size_t idx,
      const char **crc32)
{
   if (!playlist || idx >= RBUF_LEN(playlist->entries))
      return;

   if (crc32)
      *crc32 = playlist->entries[idx].crc32;
}

void playlist_get_db_name(playlist_t *playlist, size_t idx,
      const char **db_name)
{
   if (!playlist || idx >= RBUF_LEN(playlist->entries))
      return;

   if (db_name)
   {
      if (!string_is_empty(playlist->entries[idx].db_name))
         *db_name = playlist->entries[idx].db_name;
      else
      {
         const char *conf_path_basename = path_basename_nocompression(playlist->config.path);

         /* Only use file basename if this is a 'collection' playlist
          * (i.e. ignore history/favourites) */
         if (
                  !string_is_empty(conf_path_basename)
               && !string_ends_with_size(conf_path_basename, "_history.lpl",
                        strlen(conf_path_basename), STRLEN_CONST("_history.lpl"))
               && !string_is_equal(conf_path_basename,
                        FILE_PATH_CONTENT_FAVORITES)
            )
            *db_name = conf_path_basename;
      }
   }
}

const char *playlist_get_default_core_path(playlist_t *playlist)
{
   if (!playlist)
      return NULL;
   return playlist->default_core_path;
}

const char *playlist_get_default_core_name(playlist_t *playlist)
{
   if (!playlist)
      return NULL;
   return playlist->default_core_name;
}

enum playlist_label_display_mode playlist_get_label_display_mode(playlist_t *playlist)
{
   if (!playlist)
      return LABEL_DISPLAY_MODE_DEFAULT;
   return playlist->label_display_mode;
}

enum playlist_thumbnail_mode playlist_get_thumbnail_mode(
      playlist_t *playlist, enum playlist_thumbnail_id thumbnail_id)
{
   if (!playlist)
      return PLAYLIST_THUMBNAIL_MODE_DEFAULT;

   if (thumbnail_id == PLAYLIST_THUMBNAIL_RIGHT)
      return playlist->right_thumbnail_mode;
   else if (thumbnail_id == PLAYLIST_THUMBNAIL_LEFT)
      return playlist->left_thumbnail_mode;

   /* Fallback */
   return PLAYLIST_THUMBNAIL_MODE_DEFAULT;
}

enum playlist_sort_mode playlist_get_sort_mode(playlist_t *playlist)
{
   if (!playlist)
      return PLAYLIST_SORT_MODE_DEFAULT;
   return playlist->sort_mode;
}

const char *playlist_get_scan_content_dir(playlist_t *playlist)
{
   if (!playlist)
      return NULL;
   return playlist->scan_record.content_dir;
}

const char *playlist_get_scan_file_exts(playlist_t *playlist)
{
   if (!playlist)
      return NULL;
   return playlist->scan_record.file_exts;
}

const char *playlist_get_scan_dat_file_path(playlist_t *playlist)
{
   if (!playlist)
      return NULL;
   return playlist->scan_record.dat_file_path;
}

bool playlist_get_scan_search_recursively(playlist_t *playlist)
{
   if (!playlist)
      return false;
   return playlist->scan_record.search_recursively;
}

bool playlist_get_scan_search_archives(playlist_t *playlist)
{
   if (!playlist)
      return false;
   return playlist->scan_record.search_archives;
}

bool playlist_get_scan_filter_dat_content(playlist_t *playlist)
{
   if (!playlist)
      return false;
   return playlist->scan_record.filter_dat_content;
}

bool playlist_scan_refresh_enabled(playlist_t *playlist)
{
   if (!playlist)
      return false;
   return !string_is_empty(playlist->scan_record.content_dir);
}

void playlist_set_default_core_path(playlist_t *playlist, const char *core_path)
{
   char real_core_path[PATH_MAX_LENGTH];

   if (!playlist || string_is_empty(core_path))
      return;

   /* Get 'real' core path */
   strlcpy(real_core_path, core_path, sizeof(real_core_path));
   if (!string_is_equal(real_core_path, FILE_PATH_DETECT) &&
       !string_is_equal(real_core_path, FILE_PATH_BUILTIN))
       playlist_resolve_path(PLAYLIST_SAVE, true,
             real_core_path, sizeof(real_core_path));

   if (string_is_empty(real_core_path))
      return;

   if (!string_is_equal(playlist->default_core_path, real_core_path))
   {
      if (playlist->default_core_path)
         free(playlist->default_core_path);
      playlist->default_core_path = strdup(real_core_path);
      playlist->modified = true;
   }
}

void playlist_set_default_core_name(
      playlist_t *playlist, const char *core_name)
{
   if (!playlist || string_is_empty(core_name))
      return;

   if (!string_is_equal(playlist->default_core_name, core_name))
   {
      if (playlist->default_core_name)
         free(playlist->default_core_name);
      playlist->default_core_name = strdup(core_name);
      playlist->modified = true;
   }
}

void playlist_set_label_display_mode(playlist_t *playlist,
      enum playlist_label_display_mode label_display_mode)
{
   if (!playlist)
      return;

   if (playlist->label_display_mode != label_display_mode)
   {
      playlist->label_display_mode = label_display_mode;
      playlist->modified = true;
   }
}

void playlist_set_thumbnail_mode(
      playlist_t *playlist, enum playlist_thumbnail_id thumbnail_id,
      enum playlist_thumbnail_mode thumbnail_mode)
{
   if (!playlist)
      return;

   switch (thumbnail_id)
   {
      case PLAYLIST_THUMBNAIL_RIGHT:
         playlist->right_thumbnail_mode = thumbnail_mode;
         playlist->modified             = true;
         break;
      case PLAYLIST_THUMBNAIL_LEFT:
         playlist->left_thumbnail_mode = thumbnail_mode;
         playlist->modified            = true;
         break;
   }
}

void playlist_set_sort_mode(playlist_t *playlist,
      enum playlist_sort_mode sort_mode)
{
   if (!playlist)
      return;

   if (playlist->sort_mode != sort_mode)
   {
      playlist->sort_mode = sort_mode;
      playlist->modified  = true;
   }
}

void playlist_set_scan_content_dir(playlist_t *playlist, const char *content_dir)
{
   bool current_string_empty;
   bool new_string_empty;

   if (!playlist)
      return;

   current_string_empty = string_is_empty(playlist->scan_record.content_dir);
   new_string_empty     = string_is_empty(content_dir);

   /* Check whether string value has changed
    * (note that a NULL or empty argument will
    * unset the playlist value) */
   if (( current_string_empty && !new_string_empty) ||
       (!current_string_empty &&  new_string_empty) ||
       !string_is_equal(playlist->scan_record.content_dir, content_dir))
      playlist->modified = true;
   else
      return; /* Strings are identical; do nothing */

   if (playlist->scan_record.content_dir)
   {
      free(playlist->scan_record.content_dir);
      playlist->scan_record.content_dir = NULL;
   }

   if (!new_string_empty)
      playlist->scan_record.content_dir = strdup(content_dir);
}

void playlist_set_scan_file_exts(playlist_t *playlist, const char *file_exts)
{
   bool current_string_empty;
   bool new_string_empty;

   if (!playlist)
      return;

   current_string_empty = string_is_empty(playlist->scan_record.file_exts);
   new_string_empty     = string_is_empty(file_exts);

   /* Check whether string value has changed
    * (note that a NULL or empty argument will
    * unset the playlist value) */
   if (( current_string_empty && !new_string_empty) ||
       (!current_string_empty &&  new_string_empty) ||
       !string_is_equal(playlist->scan_record.file_exts, file_exts))
      playlist->modified = true;
   else
      return; /* Strings are identical; do nothing */

   if (playlist->scan_record.file_exts)
   {
      free(playlist->scan_record.file_exts);
      playlist->scan_record.file_exts = NULL;
   }

   if (!new_string_empty)
      playlist->scan_record.file_exts = strdup(file_exts);
}

void playlist_set_scan_dat_file_path(playlist_t *playlist, const char *dat_file_path)
{
   bool current_string_empty;
   bool new_string_empty;

   if (!playlist)
      return;

   current_string_empty = string_is_empty(playlist->scan_record.dat_file_path);
   new_string_empty     = string_is_empty(dat_file_path);

   /* Check whether string value has changed
    * (note that a NULL or empty argument will
    * unset the playlist value) */
   if (( current_string_empty && !new_string_empty) ||
       (!current_string_empty &&  new_string_empty) ||
       !string_is_equal(playlist->scan_record.dat_file_path, dat_file_path))
      playlist->modified = true;
   else
      return; /* Strings are identical; do nothing */

   if (playlist->scan_record.dat_file_path)
   {
      free(playlist->scan_record.dat_file_path);
      playlist->scan_record.dat_file_path = NULL;
   }

   if (!new_string_empty)
      playlist->scan_record.dat_file_path = strdup(dat_file_path);
}

void playlist_set_scan_search_recursively(playlist_t *playlist, bool search_recursively)
{
   if (!playlist)
      return;

   if (playlist->scan_record.search_recursively != search_recursively)
   {
      playlist->scan_record.search_recursively = search_recursively;
      playlist->modified = true;
   }
}

void playlist_set_scan_search_archives(playlist_t *playlist, bool search_archives)
{
   if (!playlist)
      return;

   if (playlist->scan_record.search_archives != search_archives)
   {
      playlist->scan_record.search_archives = search_archives;
      playlist->modified = true;
   }
}

void playlist_set_scan_filter_dat_content(playlist_t *playlist, bool filter_dat_content)
{
   if (!playlist)
      return;

   if (playlist->scan_record.filter_dat_content != filter_dat_content)
   {
      playlist->scan_record.filter_dat_content = filter_dat_content;
      playlist->modified = true;
   }
}

/* Returns true if specified entry has a valid
 * core association (i.e. a non-empty string
 * other than DETECT) */
bool playlist_entry_has_core(const struct playlist_entry *entry)
{
   if (!entry                                              ||
       string_is_empty(entry->core_path)                   ||
       string_is_empty(entry->core_name)                   ||
       string_is_equal(entry->core_path, FILE_PATH_DETECT) ||
       string_is_equal(entry->core_name, FILE_PATH_DETECT))
      return false;

   return true;
}

/* Fetches core info object corresponding to the
 * currently associated core of the specified
 * playlist entry.
 * Returns NULL if entry does not have a valid
 * core association */
core_info_t *playlist_entry_get_core_info(const struct playlist_entry* entry)
{
   core_info_t *core_info = NULL;

   if (!playlist_entry_has_core(entry))
      return NULL;

   /* Search for associated core */
   if (core_info_find(entry->core_path, &core_info))
      return core_info;

   return NULL;
}

/* Fetches core info object corresponding to the
 * currently associated default core of the
 * specified playlist.
 * Returns NULL if playlist does not have a valid
 * default core association */
core_info_t *playlist_get_default_core_info(playlist_t* playlist)
{
   core_info_t *core_info = NULL;

   if (!playlist ||
       string_is_empty(playlist->default_core_path) ||
       string_is_empty(playlist->default_core_name) ||
       string_is_equal(playlist->default_core_path, FILE_PATH_DETECT) ||
       string_is_equal(playlist->default_core_name, FILE_PATH_DETECT))
      return NULL;

   /* Search for associated core */
   if (core_info_find(playlist->default_core_path, &core_info))
      return core_info;

   return NULL;
}

