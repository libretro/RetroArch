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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <libretro.h>
#include <boolean.h>
#include <retro_miscellaneous.h>
#include <compat/posix_string.h>
#include <string/stdstring.h>
#include <streams/interface_stream.h>
#include <streams/file_stream.h>
#include <file/file_path.h>
#include <file/archive_file.h>
#include <lists/string_list.h>
#include <formats/rjson.h>
#include <formats/rjson_stream.h>
#include <array/rbuf.h>

#include "playlist.h"
#include "verbosity.h"
#include "file_path_special.h"
#include "core_info.h"

#if defined(ANDROID)
#include "play_feature_delivery/play_feature_delivery.h"
#include <compat/strl.h>
#ifdef __MACH__
#include <TargetConditionals.h>
#endif
#endif

#ifndef PLAYLIST_ENTRIES
#define PLAYLIST_ENTRIES 6
#endif

#define WINDOWS_PATH_DELIMITER '\\'
#define POSIX_PATH_DELIMITER '/'

/* Holds all configuration parameters required
 * to repeat a manual content scan for a
 * previously manual-scan-generated playlist */
typedef struct
{
   char *content_dir;
   char *file_exts;
   char *dat_file_path;
   char *database_name;
   bool search_recursively;
   bool search_archives;
   bool filter_dat_content;
   bool overwrite_playlist;
   bool omit_db_ref;
   int db_usage;
} playlist_manual_scan_record_t;

enum content_playlist_flags
{
   CNT_PLAYLIST_FLG_MOD        = (1 << 0),
   CNT_PLAYLIST_FLG_OLD_FMT    = (1 << 1),
   CNT_PLAYLIST_FLG_COMPRESSED = (1 << 2),
   CNT_PLAYLIST_FLG_CACHED_EXT = (1 << 3)
};

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
   enum playlist_thumbnail_match_mode thumbnail_match_mode;
   enum playlist_sort_mode sort_mode;

   /* Size of the file this playlist was read from, at the moment it was
    * read.  Used only to decide whether a cached playlist may be reused
    * rather than parsed again - see playlist_init_cached.  The VFS layer
    * exposes no modification time portably, so this is a backstop for
    * edits made outside the process; changes made inside it invalidate
    * the cache explicitly. */
   int64_t file_size;

   uint8_t flags;
};

enum json_ctx_flags
{
   JSON_CTX_FLG_IN_ITEMS             = (1 << 0),
   JSON_CTX_FLG_IN_SUBSYSTEM_CONTENT = (1 << 1),
   JSON_CTX_FLG_CAPACITY_EXCEEDED    = (1 << 2),
   JSON_CTX_FLG_OOM                  = (1 << 3)
};

typedef struct
{
   struct playlist_entry *current_entry;
   char **current_string_val;
   unsigned *current_entry_uint_val;
   enum playlist_label_display_mode *current_meta_label_display_mode_val;
   enum playlist_thumbnail_mode *current_meta_thumbnail_mode_val;
   enum playlist_thumbnail_match_mode *current_meta_thumbnail_match_mode_val;
   enum playlist_sort_mode *current_meta_sort_mode_val;
   unsigned *current_meta_db_usage_val;
   bool *current_meta_bool_val;
   playlist_t *playlist;

   unsigned array_depth;
   unsigned object_depth;

   uint8_t flags;
} JSONContext;

/* TODO/FIXME - global state - perhaps move outside this file */
static playlist_t *playlist_cached = NULL;
/* Set when the cached playlist's file has been rewritten by something
 * else, so the next request re-reads it.  See
 * playlist_cached_after_write for why this is a flag and not a free. */
static bool playlist_cached_stale  = false;

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

   playlist_cached         = pl;
   playlist_cached->flags |= CNT_PLAYLIST_FLG_CACHED_EXT;
}

/* Convenience function: copies specified playlist
 * path to specified playlist configuration object */
size_t playlist_config_set_path(playlist_config_t *config, const char *path)
{
   if (config)
   {
      if (path && *path)
         return strlcpy(config->path, path, sizeof(config->path));
      config->path[0] = '\0';
   }
   return 0;
}

/* Convenience function: copies base content directory
 * path to specified playlist configuration object.
 * Also sets autofix_paths boolean, depending on base
 * content directory value */
size_t playlist_config_set_base_content_directory(
      playlist_config_t* config, const char* path)
{
   if (config)
   {
      config->autofix_paths = path && *path;
      if (config->autofix_paths)
#if TARGET_OS_IPHONE
         return fill_pathname_abbreviate_special(
               config->base_content_directory, path,
               sizeof(config->base_content_directory));
#else
         return strlcpy(config->base_content_directory, path,
               sizeof(config->base_content_directory));
#endif
      config->base_content_directory[0] = '\0';
   }
   return 0;
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
      char *s,
      const char *in_path,
      const char *in_oldrefpath, size_t in_oldrefpath_length,
      const char *in_refpath,    size_t in_refpath_length,
      size_t len)
{
   if (  in_oldrefpath_length > 0
      && memcmp(in_path, in_oldrefpath, in_oldrefpath_length) == 0)
   {
      const char *suffix        = in_path + in_oldrefpath_length;
      size_t      suffix_length = strlen(suffix);
      size_t      required      = in_refpath_length + suffix_length + 1;

      if (required > len)
      {
         size_t in_path_length = in_oldrefpath_length + suffix_length;
         size_t copy_len       = in_path_length < (len - 1)
                               ? in_path_length : (len - 1);
         memcpy(s, in_path, copy_len);
         s[copy_len] = '\0';
         return;
      }

      /* Copy new base path prefix (already in local format) */
      memcpy(s, in_refpath, in_refpath_length);

      /* Single fused pass: copy suffix while fixing
       * path delimiters */
      {
         char  *dst = s + in_refpath_length;
         size_t i;
#ifdef _WIN32
         for (i = 0; i < suffix_length; i++)
            dst[i] = (suffix[i] == POSIX_PATH_DELIMITER)
                   ? WINDOWS_PATH_DELIMITER : suffix[i];
#else
         for (i = 0; i < suffix_length; i++)
            dst[i] = (suffix[i] == WINDOWS_PATH_DELIMITER)
                   ? POSIX_PATH_DELIMITER : suffix[i];
#endif
         dst[suffix_length] = '\0';
      }
   }
   else
      strlcpy(s, in_path, len);
}

/* Generates a case insensitive hash for the
 * specified path string */
static uint32_t playlist_path_hash(const char *path)
{
   unsigned char c;
   uint32_t hash = (uint32_t)0x811c9dc5;
   while ((c = (unsigned char)*(path++)) != '\0')
      hash = ((hash ^ (uint32_t)((c >= 'A' && c <= 'Z') ? (c | 0x20) : c)) * (uint32_t)0x01000193);
   return (hash ? hash : 1);
}

static void playlist_path_id_free(playlist_path_id_t *path_id)
{
   if (   (path_id->archive_path)
       && (path_id->archive_path != path_id->real_path))
      free(path_id->archive_path);

   if (path_id->real_path)
      free(path_id->real_path);

   free(path_id);
}

static playlist_path_id_t *playlist_path_id_init(const char *path)
{
   playlist_path_id_t *path_id  = (playlist_path_id_t*)malloc(sizeof(*path_id));

   if (!path_id)
      return NULL;

   path_id->real_path           = NULL;
   path_id->archive_path        = NULL;
   path_id->real_path_hash      = 0;
   path_id->archive_path_hash   = 0;
   path_id->is_archive          = false;
   path_id->is_in_archive       = false;

   if (path && *path)
   {
      char real_path[PATH_MAX_LENGTH];
      const char *archive_delim = NULL;
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
         char archive_path[PATH_MAX_LENGTH];
         size_t _len                 = (1 + archive_delim - real_path);
         if (_len >= PATH_MAX_LENGTH)
            _len                     = PATH_MAX_LENGTH;
         strlcpy(archive_path, real_path, _len * sizeof(char));

         path_id->archive_path       = strdup(archive_path);
         path_id->archive_path_hash  = playlist_path_hash(archive_path);
         path_id->is_in_archive      = true;
      }
      else if (path_id->is_archive)
      {
         path_id->archive_path       = path_id->real_path;
         path_id->archive_path_hash  = path_id->real_path_hash;
      }
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
   if (   (!real_path || !*real_path)
       || (!entry_path || !*entry_path)
       || !config)
      return false;

   /* Get entry 'real' path */
   strlcpy(entry_real_path, entry_path, sizeof(entry_real_path));
   playlist_resolve_path(PLAYLIST_LOAD, false,
         entry_real_path, sizeof(entry_real_path));
   path_resolve_realpath(entry_real_path, sizeof(entry_real_path), true);

   if (!*entry_real_path)
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
   real_path_is_compressed           = path_is_compressed_file(real_path);
   entry_real_path_is_compressed     = path_is_compressed_file(entry_real_path);

   if (   (real_path_is_compressed  && !entry_real_path_is_compressed)
       || (!real_path_is_compressed &&  entry_real_path_is_compressed))
   {
      const char *compressed_path_a  = real_path_is_compressed ? real_path       : entry_real_path;
      const char *full_path          = real_path_is_compressed ? entry_real_path : real_path;
      const char *delim              = path_get_archive_delim(full_path);

      if (delim)
      {
         char compressed_path_b[PATH_MAX_LENGTH];
         size_t _len = (1 + delim - full_path);
         strlcpy(compressed_path_b, full_path,
               (  _len < PATH_MAX_LENGTH 
                ? _len : PATH_MAX_LENGTH) * sizeof(char));
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
/* Compares two content path IDs under the playlist's matching rules:
 * exact real-path equality first (case-insensitive on
 * case-insensitive operating systems), then - when fuzzy archive
 * matching is enabled - the bare-archive vs inside-archive
 * equivalence on the parent archive path.  Symmetric in its
 * arguments. */
static bool playlist_path_ids_match(const playlist_path_id_t *a,
      const playlist_path_id_t *b, const playlist_config_t *config)
{
   /* Ensure we have valid real_path strings */
   if (   (!a->real_path || !*a->real_path)
       || (!b->real_path || !*b->real_path))
      return false;

   /* First pass comparison */
   if (a->real_path_hash == b->real_path_hash)
   {
#ifdef _WIN32
      /* Handle case-insensitive operating systems*/
      if (string_is_equal_noncase(a->real_path, b->real_path))
         return true;
#else
      if (string_is_equal(a->real_path, b->real_path))
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
   if (   ((a->is_archive && !a->is_in_archive) && b->is_in_archive)
       || ((b->is_archive && !b->is_in_archive) && a->is_in_archive))
   {
      /* Ensure we have valid parent archive path
       * strings */
      if (   (!a->archive_path || !*a->archive_path)
          || (!b->archive_path || !*b->archive_path))
         return false;

      if (a->archive_path_hash == b->archive_path_hash)
      {
#ifdef _WIN32
         /* Handle case-insensitive operating systems*/
         if (string_is_equal_noncase(a->archive_path, b->archive_path))
            return true;
#else
         if (string_is_equal(a->archive_path, b->archive_path))
            return true;
#endif
      }
   }

   return false;
}

static bool playlist_path_matches_entry(playlist_path_id_t *path_id,
      struct playlist_entry *entry, const playlist_config_t *config)
{
   /* Sanity check */
   if (!path_id || !entry || !config)
      return false;

   /* Check whether entry contains a path ID cache */
   if (!entry->path_id)
   {
      if (!(entry->path_id = playlist_path_id_init(entry->path)))
         return false;
   }

   return playlist_path_ids_match(path_id, entry->path_id, config);
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
static bool playlist_core_path_equal(const char *real_core_path,
      const char *entry_core_path, const playlist_config_t *config)
{
   char entry_real_core_path[PATH_MAX_LENGTH];

   /* Sanity check */
   if (     (!real_core_path || !*real_core_path)
         || (!entry_core_path || !*entry_core_path))
      return false;

   /* Get entry 'real' core path */
   strlcpy(entry_real_core_path, entry_core_path, sizeof(entry_real_core_path));
   if (   !string_is_equal(entry_real_core_path, FILE_PATH_DETECT)
       && !string_is_equal(entry_real_core_path, FILE_PATH_BUILTIN))
      playlist_resolve_path(PLAYLIST_SAVE, true, entry_real_core_path,
            sizeof(entry_real_core_path));

   if (*entry_real_core_path)
   {
#ifdef _WIN32
      /* Handle case-insensitive operating systems*/
      if (string_is_equal_noncase(real_core_path, entry_real_core_path))
         return true;
#else
      if (string_is_equal(real_core_path, entry_real_core_path))
         return true;
#endif
      if (     config->autofix_paths
            && core_info_core_file_id_is_equal(real_core_path, entry_core_path))
         return true;
   }

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

   entry->path               = NULL;
   entry->label              = NULL;
   entry->core_path          = NULL;
   entry->core_name          = NULL;
   entry->db_name            = NULL;
   entry->crc32              = NULL;
   entry->subsystem_ident    = NULL;
   entry->subsystem_name     = NULL;
   entry->runtime_str        = NULL;
   entry->last_played_str    = NULL;
   entry->subsystem_roms     = NULL;
   entry->path_id            = NULL;
   entry->entry_slot         = 0;
   entry->runtime_status     = PLAYLIST_RUNTIME_UNKNOWN;
   entry->runtime_hours      = 0;
   entry->runtime_minutes    = 0;
   entry->runtime_seconds    = 0;
   entry->last_played_year   = 0;
   entry->last_played_month  = 0;
   entry->last_played_day    = 0;
   entry->last_played_hour   = 0;
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
   size_t _len;
   struct playlist_entry *entry_to_delete;

   if (!playlist)
      return;

   _len = RBUF_LEN(playlist->entries);
   if (idx >= _len)
      return;

   /* Free unwanted entry */
   entry_to_delete = (struct playlist_entry *)(playlist->entries + idx);
   if (entry_to_delete)
      playlist_free_entry(entry_to_delete);

   /* Shift remaining entries to fill the gap */
   memmove(playlist->entries + idx, playlist->entries + idx + 1,
         (_len - 1 - idx) * sizeof(struct playlist_entry));

   RBUF_RESIZE(playlist->entries, _len - 1);

   playlist->flags |= CNT_PLAYLIST_FLG_MOD;
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
   size_t i, write_idx;
   size_t _len;
   bool deleted_any            = false;

   if (!playlist || (!search_path || !*search_path))
      return;

   if (!(path_id = playlist_path_id_init(search_path)))
      return;

   _len = RBUF_LEN(playlist->entries);

   /* Two-pointer compaction: iterate once, free matching
    * entries and shift non-matching ones down in place */
   for (i = 0, write_idx = 0; i < _len; i++)
   {
      if (playlist_path_matches_entry(path_id,
            &playlist->entries[i], &playlist->config))
      {
         /* Free the matching entry */
         playlist_free_entry(&playlist->entries[i]);
         deleted_any = true;
         continue;
      }

      /* Keep this entry — shift down if needed */
      if (write_idx != i)
         playlist->entries[write_idx] = playlist->entries[i];
      write_idx++;
   }

   if (deleted_any)
   {
      RBUF_RESIZE(playlist->entries, write_idx);
      playlist->flags |= CNT_PLAYLIST_FLG_MOD;
   }

   playlist_path_id_free(path_id);
}

void playlist_get_index_by_path(playlist_t *playlist,
      const char *search_path,
      const struct playlist_entry **entry)
{
   playlist_path_id_t *path_id = NULL;
   size_t i, _len;

   if (!playlist || !entry || (!search_path || !*search_path))
      return;

   if (!(path_id = playlist_path_id_init(search_path)))
      return;

   for (i = 0, _len = RBUF_LEN(playlist->entries); i < _len; i++)
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
   size_t i, _len;

   if (!playlist || (!path || !*path))
      return false;

   if (!(path_id = playlist_path_id_init(path)))
      return false;

   for (i = 0, _len = RBUF_LEN(playlist->entries); i < _len; i++)
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

/* ============================================================
 * Content path dedup index
 *
 * Open-addressing hash table over content path IDs, answering
 * playlist_entry_exists() queries in O(1) expected time.  Any two
 * path IDs that playlist_path_ids_match() accepts share at least
 * one hash value between their {real, archive} pairs: an exact
 * match implies equal real-path hashes, and the fuzzy bare-archive
 * vs inside-archive match implies equal parent-archive hashes,
 * where a bare archive's archive hash equals its real hash.
 * Indexing every ID under both of its hashes and probing under
 * both of the query's hashes therefore yields a candidate superset,
 * and each candidate is verified with playlist_path_ids_match()
 * itself - the index can never change an answer, only the cost.
 *
 * The index owns every path ID it stores (entries are re-derived
 * from the entry path strings during seeding, never borrowed from
 * the entries' lazy caches), so entry deletion and playlist
 * teardown cannot dangle it.  Any allocation failure degrades the
 * index permanently: queries fall back to the linear
 * playlist_entry_exists() scan and stay correct.
 * ============================================================ */

typedef struct
{
   uint32_t hash;
   playlist_path_id_t *pid;   /* non-owning; NULL = empty slot */
} playlist_dedup_slot_t;

struct playlist_dedup
{
   playlist_dedup_slot_t *table;   /* power-of-two slot count */
   size_t table_cap;
   size_t table_used;
   playlist_path_id_t **owned;     /* every stored ID, exactly once */
   size_t owned_cnt;
   size_t owned_cap;
   size_t seed_pos;                /* next entry index to seed */
   bool seeded;
   bool degraded;
};

static void playlist_dedup_degrade(playlist_dedup_t *dedup)
{
   if (dedup->table)
      free(dedup->table);
   dedup->table      = NULL;
   dedup->table_cap  = 0;
   dedup->table_used = 0;
   dedup->degraded   = true;
}

/* Places @pid into the table under @hash; assumes capacity headroom
 * was already ensured. */
static void playlist_dedup_place(playlist_dedup_slot_t *table,
      size_t cap, uint32_t hash, playlist_path_id_t *pid)
{
   size_t idx = (size_t)hash & (cap - 1);
   while (table[idx].pid)
      idx = (idx + 1) & (cap - 1);
   table[idx].hash = hash;
   table[idx].pid  = pid;
}

/* True when @pid indexes under two distinct hash values (a path
 * inside an archive: parent hash differs from the full hash). */
static bool playlist_dedup_two_hashes(const playlist_path_id_t *pid)
{
   return pid->is_in_archive
       && (pid->archive_path_hash != pid->real_path_hash);
}

/* Ensures room for up to two more slots, growing (or degrading)
 * as required.  Returns false when the index became degraded. */
static bool playlist_dedup_reserve(playlist_dedup_t *dedup)
{
   playlist_dedup_slot_t *grown = NULL;
   size_t new_cap;
   size_t i;

   if (dedup->degraded)
      return false;

   /* Keep load at or below 3/4 after inserting two slots */
   if ((dedup->table_used + 2) * 4 <= dedup->table_cap * 3)
      return true;

   new_cap = (dedup->table_cap == 0) ? 64 : (dedup->table_cap << 1);
   if (!(grown = (playlist_dedup_slot_t*)calloc(new_cap, sizeof(*grown))))
   {
      playlist_dedup_degrade(dedup);
      return false;
   }

   for (i = 0; i < dedup->owned_cnt; i++)
   {
      playlist_path_id_t *pid = dedup->owned[i];
      playlist_dedup_place(grown, new_cap, pid->real_path_hash, pid);
      if (playlist_dedup_two_hashes(pid))
         playlist_dedup_place(grown, new_cap, pid->archive_path_hash, pid);
   }

   if (dedup->table)
      free(dedup->table);
   dedup->table     = grown;
   dedup->table_cap = new_cap;
   return true;
}

/* Takes ownership of @pid and indexes it under its hash(es).
 * On failure the index is degraded and @pid is freed. */
static void playlist_dedup_insert(playlist_dedup_t *dedup,
      playlist_path_id_t *pid)
{
   /* IDs with no real path can never satisfy the matcher;
    * do not store them */
   if (!pid->real_path || !*pid->real_path)
   {
      playlist_path_id_free(pid);
      return;
   }

   if (!playlist_dedup_reserve(dedup))
   {
      playlist_path_id_free(pid);
      return;
   }

   if (dedup->owned_cnt == dedup->owned_cap)
   {
      size_t new_cap = (dedup->owned_cap == 0) ? 64 : (dedup->owned_cap << 1);
      playlist_path_id_t **grown = (playlist_path_id_t**)realloc(
            dedup->owned, new_cap * sizeof(*grown));
      if (!grown)
      {
         playlist_dedup_degrade(dedup);
         playlist_path_id_free(pid);
         return;
      }
      dedup->owned     = grown;
      dedup->owned_cap = new_cap;
   }
   dedup->owned[dedup->owned_cnt++] = pid;

   playlist_dedup_place(dedup->table, dedup->table_cap,
         pid->real_path_hash, pid);
   dedup->table_used++;
   if (playlist_dedup_two_hashes(pid))
   {
      playlist_dedup_place(dedup->table, dedup->table_cap,
            pid->archive_path_hash, pid);
      dedup->table_used++;
   }
}

/* Probes the chain of @hash for a stored ID matching @probe. */
static bool playlist_dedup_probe_hash(playlist_dedup_t *dedup,
      const playlist_path_id_t *probe, uint32_t hash,
      const playlist_config_t *config)
{
   size_t idx = (size_t)hash & (dedup->table_cap - 1);
   while (dedup->table[idx].pid)
   {
      if (   dedup->table[idx].hash == hash
          && playlist_path_ids_match(probe, dedup->table[idx].pid, config))
         return true;
      idx = (idx + 1) & (dedup->table_cap - 1);
   }
   return false;
}

static bool playlist_dedup_lookup(playlist_dedup_t *dedup,
      const playlist_path_id_t *probe, const playlist_config_t *config)
{
   if (!dedup->table_cap)
      return false;
   if (playlist_dedup_probe_hash(dedup, probe,
         probe->real_path_hash, config))
      return true;
   if (playlist_dedup_two_hashes(probe))
      return playlist_dedup_probe_hash(dedup, probe,
            probe->archive_path_hash, config);
   return false;
}

playlist_dedup_t *playlist_dedup_init(void)
{
   return (playlist_dedup_t*)calloc(1, sizeof(playlist_dedup_t));
}

bool playlist_dedup_seed_step(playlist_dedup_t *dedup,
      playlist_t *playlist,
      bool (*budget_cb)(void *userdata), void *userdata)
{
   size_t _len;
   bool progressed = false;

   if (!dedup || dedup->seeded || dedup->degraded)
      return true;
   if (!playlist)
   {
      dedup->seeded = true;
      return true;
   }

   _len = RBUF_LEN(playlist->entries);
   while (dedup->seed_pos < _len)
   {
      playlist_path_id_t *pid;

      /* Guarantee forward progress: seed at least one entry per
       * call before consulting the budget */
      if (progressed && budget_cb && !budget_cb(userdata))
         return false;

      if (!(pid = playlist_path_id_init(
            playlist->entries[dedup->seed_pos].path)))
      {
         playlist_dedup_degrade(dedup);
         return true;
      }
      playlist_dedup_insert(dedup, pid);
      if (dedup->degraded)
         return true;

      dedup->seed_pos++;
      progressed = true;
   }

   dedup->seeded = true;
   return true;
}

bool playlist_dedup_check_add(playlist_dedup_t *dedup,
      playlist_t *playlist, const char *path, bool will_add)
{
   playlist_path_id_t *path_id = NULL;
   bool found                  = false;

   if (!playlist || (!path || !*path))
      return false;

   if (!dedup || dedup->degraded)
      return playlist_entry_exists(playlist, path);

   if (!(path_id = playlist_path_id_init(path)))
      return false;

   found = playlist_dedup_lookup(dedup, path_id, &playlist->config);

   if (!found && will_add)
      playlist_dedup_insert(dedup, path_id);   /* consumes path_id */
   else
      playlist_path_id_free(path_id);

   return found;
}

void playlist_dedup_free(playlist_dedup_t *dedup)
{
   size_t i;
   if (!dedup)
      return;
   for (i = 0; i < dedup->owned_cnt; i++)
      playlist_path_id_free(dedup->owned[i]);
   if (dedup->owned)
      free(dedup->owned);
   if (dedup->table)
      free(dedup->table);
   free(dedup);
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

      playlist->flags |= CNT_PLAYLIST_FLG_MOD;
   }

   if (update_entry->label && (update_entry->label != entry->label))
   {
      if (entry->label)
         free(entry->label);
      entry->label       = strdup(update_entry->label);
      playlist->flags   |= CNT_PLAYLIST_FLG_MOD;
   }

   if (update_entry->core_path && (update_entry->core_path != entry->core_path))
   {
      if (entry->core_path)
         free(entry->core_path);
      entry->core_path   = strdup(update_entry->core_path);
      playlist->flags   |= CNT_PLAYLIST_FLG_MOD;
   }

   if (update_entry->core_name && (update_entry->core_name != entry->core_name))
   {
      if (entry->core_name)
         free(entry->core_name);
      entry->core_name   = strdup(update_entry->core_name);
      playlist->flags   |= CNT_PLAYLIST_FLG_MOD;
   }

   if (update_entry->db_name && (update_entry->db_name != entry->db_name))
   {
      if (entry->db_name)
         free(entry->db_name);
      entry->db_name     = strdup(update_entry->db_name);
      playlist->flags   |= CNT_PLAYLIST_FLG_MOD;
   }

   if (update_entry->crc32 && (update_entry->crc32 != entry->crc32))
   {
      if (entry->crc32)
         free(entry->crc32);
      entry->crc32       = strdup(update_entry->crc32);
      playlist->flags   |= CNT_PLAYLIST_FLG_MOD;
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

      if (register_update)
         playlist->flags   |= CNT_PLAYLIST_FLG_MOD;
   }

   if (update_entry->core_path && (update_entry->core_path != entry->core_path))
   {
      if (entry->core_path)
         free(entry->core_path);
      entry->core_path      = strdup(update_entry->core_path);
      if (register_update)
         playlist->flags   |= CNT_PLAYLIST_FLG_MOD;
   }

   if (update_entry->runtime_status != entry->runtime_status)
   {
      entry->runtime_status = update_entry->runtime_status;
      if (register_update)
         playlist->flags   |= CNT_PLAYLIST_FLG_MOD;
   }

   if (update_entry->runtime_hours != entry->runtime_hours)
   {
      entry->runtime_hours = update_entry->runtime_hours;
      if (register_update)
         playlist->flags   |= CNT_PLAYLIST_FLG_MOD;
   }

   if (update_entry->runtime_minutes != entry->runtime_minutes)
   {
      entry->runtime_minutes = update_entry->runtime_minutes;
      if (register_update)
         playlist->flags   |= CNT_PLAYLIST_FLG_MOD;
   }

   if (update_entry->runtime_seconds != entry->runtime_seconds)
   {
      entry->runtime_seconds = update_entry->runtime_seconds;
      if (register_update)
         playlist->flags   |= CNT_PLAYLIST_FLG_MOD;
   }

   if (update_entry->last_played_year != entry->last_played_year)
   {
      entry->last_played_year = update_entry->last_played_year;
      if (register_update)
         playlist->flags   |= CNT_PLAYLIST_FLG_MOD;
   }

   if (update_entry->last_played_month != entry->last_played_month)
   {
      entry->last_played_month = update_entry->last_played_month;
      if (register_update)
         playlist->flags   |= CNT_PLAYLIST_FLG_MOD;
   }

   if (update_entry->last_played_day != entry->last_played_day)
   {
      entry->last_played_day = update_entry->last_played_day;
      if (register_update)
         playlist->flags   |= CNT_PLAYLIST_FLG_MOD;
   }

   if (update_entry->last_played_hour != entry->last_played_hour)
   {
      entry->last_played_hour = update_entry->last_played_hour;
      if (register_update)
         playlist->flags   |= CNT_PLAYLIST_FLG_MOD;
   }

   if (update_entry->last_played_minute != entry->last_played_minute)
   {
      entry->last_played_minute = update_entry->last_played_minute;
      if (register_update)
         playlist->flags   |= CNT_PLAYLIST_FLG_MOD;
   }

   if (update_entry->last_played_second != entry->last_played_second)
   {
      entry->last_played_second = update_entry->last_played_second;
      if (register_update)
         playlist->flags   |= CNT_PLAYLIST_FLG_MOD;
   }

   if (update_entry->runtime_str && (update_entry->runtime_str != entry->runtime_str))
   {
      if (entry->runtime_str)
         free(entry->runtime_str);
      entry->runtime_str    = strdup(update_entry->runtime_str);
      if (register_update)
         playlist->flags   |= CNT_PLAYLIST_FLG_MOD;
   }

   if (update_entry->last_played_str && (update_entry->last_played_str != entry->last_played_str))
   {
      if (entry->last_played_str)
         free(entry->last_played_str);
      entry->last_played_str = NULL;
      entry->last_played_str = strdup(update_entry->last_played_str);
      if (register_update)
         playlist->flags    |= CNT_PLAYLIST_FLG_MOD;
   }
}

bool playlist_push_runtime(playlist_t *playlist,
      const struct playlist_entry *entry)
{
   playlist_path_id_t *path_id = NULL;
   size_t i, _len;
   char real_core_path[PATH_MAX_LENGTH];

   if (!playlist || !entry)
      goto error;

   if (!entry->core_path || !*entry->core_path)
   {
      RARCH_ERR("[Playlist] Cannot push NULL or empty core path into the playlist.\n");
      goto error;
   }

   /* Get path ID */
   if (!(path_id = playlist_path_id_init(entry->path)))
      goto error;

   /* Get 'real' core path */
   strlcpy(real_core_path, entry->core_path, sizeof(real_core_path));
   if (   !string_is_equal(real_core_path, FILE_PATH_DETECT)
       && !string_is_equal(real_core_path, FILE_PATH_BUILTIN))
      playlist_resolve_path(PLAYLIST_SAVE, true, real_core_path,
             sizeof(real_core_path));

   if (!*real_core_path)
   {
      RARCH_ERR("[Playlist] Cannot push NULL or empty core path into the playlist.\n");
      goto error;
   }

   _len = RBUF_LEN(playlist->entries);
   for (i = 0; i < _len; i++)
   {
      struct playlist_entry tmp;
      bool equal_path  = ((!path_id->real_path || !*path_id->real_path)
            && (!playlist->entries[i].path || *playlist->entries[i].path));

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

   if (_len == playlist->config.capacity)
   {
      struct playlist_entry *last_entry = &playlist->entries[_len - 1];
      playlist_free_entry(last_entry);
      _len--;
   }
   else
   {
      /* Allocate memory to fit one more item and resize the buffer */
      if (!RBUF_TRYFIT(playlist->entries, _len + 1))
         goto error; /* out of memory */
      RBUF_RESIZE(playlist->entries, _len + 1);
   }

   if (playlist->entries)
   {
      memmove(playlist->entries + 1, playlist->entries,
            _len * sizeof(struct playlist_entry));

      /* Zero all fields to avoid stale data from shifted entries */
      memset(&playlist->entries[0], 0, sizeof(struct playlist_entry));

      if (path_id->real_path && *path_id->real_path)
         playlist->entries[0].path            = strdup(path_id->real_path);
      playlist->entries[0].path_id            = path_id;
      path_id                                 = NULL;

      if (*real_core_path)
         playlist->entries[0].core_path       = strdup(real_core_path);

      playlist->entries[0].runtime_status     = entry->runtime_status;
      playlist->entries[0].runtime_hours      = entry->runtime_hours;
      playlist->entries[0].runtime_minutes    = entry->runtime_minutes;
      playlist->entries[0].runtime_seconds    = entry->runtime_seconds;
      playlist->entries[0].last_played_year   = entry->last_played_year;
      playlist->entries[0].last_played_month  = entry->last_played_month;
      playlist->entries[0].last_played_day    = entry->last_played_day;
      playlist->entries[0].last_played_hour   = entry->last_played_hour;
      playlist->entries[0].last_played_minute = entry->last_played_minute;
      playlist->entries[0].last_played_second = entry->last_played_second;

      if (entry->runtime_str && *entry->runtime_str)
         playlist->entries[0].runtime_str     = strdup(entry->runtime_str);
      if (entry->last_played_str && *entry->last_played_str)
         playlist->entries[0].last_played_str = strdup(entry->last_played_str);
   }

success:
   if (path_id)
      playlist_path_id_free(path_id);
   playlist->flags   |= CNT_PLAYLIST_FLG_MOD;
   return true;

error:
   if (path_id)
      playlist_path_id_free(path_id);
   return false;
}

void playlist_update_thumbnail_name_flag(playlist_t *playlist, size_t idx,
      enum playlist_thumbnail_name_flags thumbnail_flags)
{
   struct playlist_entry *entry = NULL;

   if (!playlist || idx >= RBUF_LEN(playlist->entries))
      return;

   entry                   = &playlist->entries[idx];
   entry->thumbnail_flags |= thumbnail_flags;
}

enum playlist_thumbnail_name_flags playlist_get_curr_thumbnail_name_flag(playlist_t *playlist, size_t idx)
{
   struct playlist_entry *entry = NULL;
   if (!playlist || idx >= RBUF_LEN(playlist->entries))
      return    PLAYLIST_THUMBNAIL_FLAG_NONE;
   entry = &playlist->entries[idx];
   return (enum playlist_thumbnail_name_flags)entry->thumbnail_flags;
}


enum playlist_thumbnail_name_flags playlist_get_next_thumbnail_name_flag(playlist_t *playlist, size_t idx)
{
   struct playlist_entry *entry = NULL;

   if (!playlist || idx >= RBUF_LEN(playlist->entries))
      return    PLAYLIST_THUMBNAIL_FLAG_NONE;
   entry = (struct playlist_entry*)&playlist->entries[idx];

   if (entry->thumbnail_flags & PLAYLIST_THUMBNAIL_FLAG_SHORT_NAME)
            return PLAYLIST_THUMBNAIL_FLAG_NONE;
   if (entry->thumbnail_flags & PLAYLIST_THUMBNAIL_FLAG_STD_NAME)
            return PLAYLIST_THUMBNAIL_FLAG_SHORT_NAME;
   if (entry->thumbnail_flags & PLAYLIST_THUMBNAIL_FLAG_FULL_NAME)
            return PLAYLIST_THUMBNAIL_FLAG_STD_NAME;
   /* Special case: only one entry in playlist, only one query is possible
    * as flag swapping relies on going back and forth among entries
    * so just use the most likely version here */
   if (idx == 0 && RBUF_LEN(playlist->entries) == 1)
            return PLAYLIST_THUMBNAIL_FLAG_STD_NAME;
   return PLAYLIST_THUMBNAIL_FLAG_FULL_NAME;
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
      bool is_core, char *s, size_t len)
{
   bool resolve_symlinks = true;

#if TARGET_OS_IPHONE
   char tmp[PATH_MAX_LENGTH];

   fill_pathname_expand_special(tmp, s, sizeof(tmp));
   /* This is probably safe for all platforms, it should end up being just a
    * lot of string copies without changing it */
   if (mode == PLAYLIST_LOAD)
      strlcpy(s, tmp, len);
   else
   {
      /* Try to expand the path to ensure that it gets saved correctly. The path
       * can be abbreviated if saving to a playlist from another playlist (ex:
       * content history to favorites). This is probably safe for all
       * platforms */
      path_resolve_realpath(tmp, sizeof(tmp), resolve_symlinks);
      /* iOS requries this because the full path can change after app update;
       * it's probably safe for all platforms... */
      fill_pathname_abbreviate_special(s, tmp, len);
   }
#else
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

   path_resolve_realpath(s, len, resolve_symlinks);
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
#if TARGET_OS_IPHONE
   char expanded_path[PATH_MAX_LENGTH];
#endif
   /* Sanity check */
   if (!path || !*path)
      return false;
#if TARGET_OS_IPHONE
   fill_pathname_expand_special(expanded_path, path, sizeof(expanded_path));
   path = expanded_path;
#endif

   /* If content is inside an archive, special
    * handling is required... */
   if (path_contains_compressed_file(path))
   {
      char archive_path[PATH_MAX_LENGTH];
      const char *delim                  = path_get_archive_delim(path);
      size_t _len                        = 0;
      struct string_list *archive_list   = NULL;
      const char *content_file           = NULL;
      bool content_found                 = false;

      if (!delim)
         return false;

      /* Get path of 'parent' archive file */
      _len = (size_t)(1 + delim - path);
      if (_len < PATH_MAX_LENGTH)
         strlcpy(archive_path, path, _len * sizeof(char));
      else
         strlcpy(archive_path, path,
               PATH_MAX_LENGTH * sizeof(char));

      /* Check if archive itself exists */
      if (!path_is_valid(archive_path))
         return false;

      /* Check if file exists inside archive */
      if (!(archive_list = file_archive_get_file_list(archive_path, NULL)))
         return false;

      /* > Get playlist entry content file name
       *   (sans archive file path) */
      content_file = delim;
      content_file++;

      if (content_file && *content_file)
      {
         size_t i;

         /* > Loop over archive file contents */
         for (i = 0; i < archive_list->size; i++)
         {
            const char *archive_file = archive_list->elems[i].data;
            if (!archive_file || !*archive_file)
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
   return path_is_valid(path);
}

/**
 * playlist_push:
 * @playlist           : Playlist handle.
 *
 * Push entry to top of playlist.
 **/
/* Shared validation preamble of playlist_push() and
 * playlist_push_unchecked(): checks the core path, builds the path
 * ID (returned to the caller, who owns it), resolves the real core
 * path into @real_core_path and derives the core name into
 * @core_name (which may point at @core_name_buf or at the entry's
 * own string).  Returns false, having freed nothing but its own
 * partial work, when the entry cannot legally be pushed. */
static bool playlist_push_prepare(playlist_t *playlist,
      const struct playlist_entry *entry,
      playlist_path_id_t **path_id,
      char *real_core_path, size_t real_core_path_size,
      char *core_name_buf, size_t core_name_buf_size,
      const char **core_name)
{
   *path_id   = NULL;
   *core_name = entry ? entry->core_name : NULL;

   if (!playlist || !entry)
      return false;

   if (!entry->core_path || !*entry->core_path)
   {
      RARCH_ERR("[Playlist] Cannot push NULL or empty core path into the playlist.\n");
      return false;
   }

   /* Get path ID */
   if (!(*path_id = playlist_path_id_init(entry->path)))
      return false;

   /* Get 'real' core path */
   strlcpy(real_core_path, entry->core_path, real_core_path_size);
   if (   !string_is_equal(real_core_path, FILE_PATH_DETECT)
       && !string_is_equal(real_core_path, FILE_PATH_BUILTIN))
      playlist_resolve_path(PLAYLIST_SAVE, true, real_core_path,
             real_core_path_size);

   if (!*real_core_path)
   {
      RARCH_ERR("[Playlist] Cannot push NULL or empty core path into the playlist.\n");
      goto error;
   }

   if (!*core_name || !**core_name)
   {
      core_name_buf[0] = '\0';
      fill_pathname(core_name_buf, path_basename(real_core_path), "",
            core_name_buf_size);

      *core_name = core_name_buf;

      if (!*core_name || !**core_name)
      {
         RARCH_ERR("[Playlist] Cannot push NULL or empty core name into the playlist.\n");
         goto error;
      }
   }

   return true;

error:
   playlist_path_id_free(*path_id);
   *path_id = NULL;
   return false;
}

/* Appends @entry at the front of @playlist without searching for an
 * existing match.  @path_id ownership transfers to the new entry on
 * success and stays with the caller on failure.  Applies the
 * capacity policy (evicting the oldest entry when full) and marks
 * the playlist modified. */
static bool playlist_push_new_entry(playlist_t *playlist,
      const struct playlist_entry *entry,
      playlist_path_id_t *path_id,
      const char *real_core_path, const char *core_name)
{
   size_t i;
   size_t _len = RBUF_LEN(playlist->entries);

   if (playlist->config.capacity == 0)
      return false;

   if (_len == playlist->config.capacity)
   {
      struct playlist_entry *last_entry = &playlist->entries[_len - 1];
      playlist_free_entry(last_entry);
      _len--;
   }
   else
   {
      /* Allocate memory to fit one more item and resize the buffer */
      if (!RBUF_TRYFIT(playlist->entries, _len + 1))
         return false; /* out of memory */
      RBUF_RESIZE(playlist->entries, _len + 1);
   }

   if (playlist->entries)
   {
      memmove(playlist->entries + 1, playlist->entries,
            _len * sizeof(struct playlist_entry));

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

      if (path_id->real_path && *path_id->real_path)
         playlist->entries[0].path            = strdup(path_id->real_path);
      playlist->entries[0].path_id            = path_id;

      playlist->entries[0].entry_slot         = entry->entry_slot;

      if (entry->label && *entry->label)
         playlist->entries[0].label           = strdup(entry->label);
      if (*real_core_path)
         playlist->entries[0].core_path       = strdup(real_core_path);
      if (core_name && *core_name)
         playlist->entries[0].core_name       = strdup(core_name);
      if (entry->db_name && *entry->db_name)
         playlist->entries[0].db_name         = strdup(entry->db_name);
      if (entry->crc32 && *entry->crc32)
         playlist->entries[0].crc32           = strdup(entry->crc32);
      if (entry->subsystem_ident && *entry->subsystem_ident)
         playlist->entries[0].subsystem_ident = strdup(entry->subsystem_ident);
      if (entry->subsystem_name && *entry->subsystem_name)
         playlist->entries[0].subsystem_name  = strdup(entry->subsystem_name);

      if (entry->subsystem_roms)
      {
         union string_list_elem_attr attributes = {0};

         playlist->entries[0].subsystem_roms    = string_list_new();

         for (i = 0; i < entry->subsystem_roms->size; i++)
            string_list_append(playlist->entries[0].subsystem_roms, entry->subsystem_roms->elems[i].data, attributes);
      }
   }

   playlist->flags   |= CNT_PLAYLIST_FLG_MOD;
   return true;
}

bool playlist_push_unchecked(playlist_t *playlist,
      const struct playlist_entry *entry)
{
   char real_core_path[PATH_MAX_LENGTH];
   char base_path[NAME_MAX_LENGTH];
   playlist_path_id_t *path_id = NULL;
   const char *core_name       = NULL;

   if (!playlist_push_prepare(playlist, entry, &path_id,
         real_core_path, sizeof(real_core_path),
         base_path, sizeof(base_path), &core_name))
      return false;

   if (!playlist_push_new_entry(playlist, entry, path_id,
         real_core_path, core_name))
   {
      playlist_path_id_free(path_id);
      return false;
   }

   return true;
}

bool playlist_push(playlist_t *playlist,
      const struct playlist_entry *entry)
{
   size_t i, _len;
   char real_core_path[PATH_MAX_LENGTH];
   char base_path[NAME_MAX_LENGTH];
   playlist_path_id_t *path_id = NULL;
   const char *core_name       = NULL;
   bool entry_updated          = false;

   if (!playlist_push_prepare(playlist, entry, &path_id,
         real_core_path, sizeof(real_core_path),
         base_path, sizeof(base_path), &core_name))
      goto error;

   _len = RBUF_LEN(playlist->entries);
   for (i = 0; i < _len; i++)
   {
      struct playlist_entry tmp;
      bool equal_path  = ((!path_id->real_path || !*path_id->real_path)
                       && (!playlist->entries[i].path || !*playlist->entries[i].path));

      equal_path       = equal_path || playlist_path_matches_entry(
            path_id, &playlist->entries[i], &playlist->config);

      if (!equal_path)
         continue;

      /* Core name can have changed while still being the same core.
       * Differentiate based on the core path only. */
      if (!playlist_core_path_equal(real_core_path, playlist->entries[i].core_path, &playlist->config))
         continue;

      if (     (entry->subsystem_ident && *entry->subsystem_ident)
            && (playlist->entries[i].subsystem_ident && *playlist->entries[i].subsystem_ident)
            && !string_is_equal(playlist->entries[i].subsystem_ident, entry->subsystem_ident))
         continue;

      if (      (!entry->subsystem_ident || !*entry->subsystem_ident)
            && (playlist->entries[i].subsystem_ident && *playlist->entries[i].subsystem_ident))
         continue;

      if (    (entry->subsystem_ident && *entry->subsystem_ident)
            && (!playlist->entries[i].subsystem_ident || !*playlist->entries[i].subsystem_ident))
         continue;

      if (     (entry->subsystem_name && *entry->subsystem_name)
            && (playlist->entries[i].subsystem_name && *playlist->entries[i].subsystem_name)
            && !string_is_equal(playlist->entries[i].subsystem_name, entry->subsystem_name))
         continue;

      if (      (!entry->subsystem_name || !*entry->subsystem_name)
            && (playlist->entries[i].subsystem_name && *playlist->entries[i].subsystem_name))
         continue;

      if (      (entry->subsystem_name && *entry->subsystem_name)
            &&  (!playlist->entries[i].subsystem_name || !*playlist->entries[i].subsystem_name))
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

            if (entry->subsystem_roms->elems[j].data && *entry->subsystem_roms->elems[j].data)
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

      /* Only write non-redundant entry slot numbers */
      if (     playlist->entries[i].entry_slot != entry->entry_slot
            && (int)entry->entry_slot > 0)
      {
         playlist->entries[i].entry_slot  = entry->entry_slot;
         entry_updated                    = true;
      }

      /* If content was previously loaded via file browser
       * or command line, certain entry values will be missing.
       * If we are now loading the same content from a playlist,
       * fill in any blanks */
      if (     !playlist->entries[i].label
            && (entry->label && *entry->label))
      {
         playlist->entries[i].label       = strdup(entry->label);
         entry_updated                    = true;
      }
      if (     !playlist->entries[i].crc32
            && (entry->crc32 && *entry->crc32))
      {
         playlist->entries[i].crc32       = strdup(entry->crc32);
         entry_updated                    = true;
      }
      if (     !playlist->entries[i].db_name
            && (entry->db_name && *entry->db_name))
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

   if (!playlist_push_new_entry(playlist, entry, path_id,
         real_core_path, core_name))
      goto error;
   path_id = NULL;   /* ownership transferred to the new entry */

success:
   if (path_id)
      playlist_path_id_free(path_id);
   playlist->flags   |= CNT_PLAYLIST_FLG_MOD;
   return true;

error:
   if (path_id)
      playlist_path_id_free(path_id);
   return false;
}


void playlist_write_runtime_file(playlist_t *playlist)
{
   size_t i, _len;
   intfstream_t *file  = NULL;
   rjsonwriter_t* writer = NULL;
   bool wrote_ok       = false;
   char write_path[PATH_MAX_LENGTH];

   if (!playlist || !(playlist->flags & CNT_PLAYLIST_FLG_MOD))
      return;

   /* Write to a temporary and move it into place, the same way
    * playlist_write_file does.  The runtime file holds accumulated
    * play time and last-played stamps for every entry, so a crash, a
    * power loss or a full disk part way through the write would
    * otherwise truncate history that cannot be recovered - and this
    * file is rewritten every time content is closed.  The temporary
    * sits in the same directory so the move stays within one
    * filesystem. */
   _len = strlcpy(write_path, playlist->config.path, sizeof(write_path));
   if (_len + STRLEN_CONST(".tmp") >= sizeof(write_path))
   {
      RARCH_ERR("[Playlist] Path too long to write safely: \"%s\".\n",
            playlist->config.path);
      return;
   }
   strlcpy_lit(write_path + _len, ".tmp", sizeof(write_path) - _len);

   if (!(file = intfstream_open_file(write_path,
         RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE)))
   {
      RARCH_ERR("[Playlist] Failed to write to file: \"%s\".\n", playlist->config.path);
      return;
   }

   if (!(writer = rjsonwriter_open_intfstream(file)))
   {
      RARCH_ERR("[Playlist] Failed to create JSON writer.\n");
      goto end;
   }

   rjsonwriter_raw(writer, "{\n", 2);
   rjsonwriter_add_spaces(writer, 2);
   rjsonwriter_add_string(writer, "version");
   rjsonwriter_raw(writer, ": ", 2);
   rjsonwriter_add_string(writer, "1.0");
   rjsonwriter_raw(writer, ",\n", 2);
   rjsonwriter_add_spaces(writer, 2);
   rjsonwriter_add_string(writer, "items");
   rjsonwriter_raw(writer, ": [\n", 4);

   for (i = 0, _len = RBUF_LEN(playlist->entries); i < _len; i++)
   {
      rjsonwriter_add_spaces(writer, 4);
      rjsonwriter_raw(writer, "{\n", 2);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "path");
      rjsonwriter_raw(writer, ": ", 2);
      rjsonwriter_add_string(writer, playlist->entries[i].path);
      rjsonwriter_raw(writer, ",\n", 2);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "core_path");
      rjsonwriter_raw(writer, ": ", 2);
      rjsonwriter_add_string(writer, playlist->entries[i].core_path);
      rjsonwriter_raw(writer, ",\n", 2);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "runtime_hours");
      rjsonwriter_raw(writer, ": ", 2);
      rjsonwriter_rawf(writer, "%u", playlist->entries[i].runtime_hours);
      rjsonwriter_raw(writer, ",\n", 2);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "runtime_minutes");
      rjsonwriter_raw(writer, ": ", 2);
      rjsonwriter_rawf(writer, "%u", playlist->entries[i].runtime_minutes);
      rjsonwriter_raw(writer, ",\n", 2);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "runtime_seconds");
      rjsonwriter_raw(writer, ": ", 2);
      rjsonwriter_rawf(writer, "%u", playlist->entries[i].runtime_seconds);
      rjsonwriter_raw(writer, ",\n", 2);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "last_played_year");
      rjsonwriter_raw(writer, ": ", 2);
      rjsonwriter_rawf(writer, "%u", playlist->entries[i].last_played_year);
      rjsonwriter_raw(writer, ",\n", 2);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "last_played_month");
      rjsonwriter_raw(writer, ": ", 2);
      rjsonwriter_rawf(writer, "%u", playlist->entries[i].last_played_month);
      rjsonwriter_raw(writer, ",\n", 2);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "last_played_day");
      rjsonwriter_raw(writer, ": ", 2);
      rjsonwriter_rawf(writer, "%u", playlist->entries[i].last_played_day);
      rjsonwriter_raw(writer, ",\n", 2);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "last_played_hour");
      rjsonwriter_raw(writer, ": ", 2);
      rjsonwriter_rawf(writer, "%u", playlist->entries[i].last_played_hour);
      rjsonwriter_raw(writer, ",\n", 2);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "last_played_minute");
      rjsonwriter_raw(writer, ": ", 2);
      rjsonwriter_rawf(writer, "%u", playlist->entries[i].last_played_minute);
      rjsonwriter_raw(writer, ",\n", 2);

      rjsonwriter_add_spaces(writer, 6);
      rjsonwriter_add_string(writer, "last_played_second");
      rjsonwriter_raw(writer, ": ", 2);
      rjsonwriter_rawf(writer, "%u", playlist->entries[i].last_played_second);
      rjsonwriter_raw(writer, "\n", 1);

      rjsonwriter_add_spaces(writer, 4);
      rjsonwriter_raw(writer, "}", 1);

      if (i < _len - 1)
         rjsonwriter_raw(writer, ",", 1);

      rjsonwriter_raw(writer, "\n", 1);
   }

   rjsonwriter_add_spaces(writer, 2);
   rjsonwriter_raw(writer, "]\n}\n", 4);

   /* The writer's own failure is the signal that the temporary is
    * incomplete; without it a short write would be moved over good
    * runtime data, and clearing the modified flag below would tell
    * the caller the data was saved when it was lost. */
   if (!(wrote_ok = rjsonwriter_free(writer)))
      RARCH_ERR("[Playlist] Failed to write to file: \"%s\".\n",
            write_path);
   writer                    = NULL;

   if (wrote_ok)
      playlist->flags       &= ~(CNT_PLAYLIST_FLG_MOD
                               | CNT_PLAYLIST_FLG_OLD_FMT
                               | CNT_PLAYLIST_FLG_COMPRESSED);

end:
   intfstream_close(file);
   free(file);

   /* Only now does the new content replace the old one.  If anything
    * above failed, the temporary is discarded and what is on disk is
    * exactly what it was. */
   if (wrote_ok && filestream_rename(write_path, playlist->config.path) == 0)
      RARCH_DBG("[Playlist] Runtime written to file: \"%s\".\n",
            playlist->config.path);
   else
   {
      filestream_delete(write_path);
      RARCH_ERR("[Playlist] Failed to write runtime file: \"%s\".\n",
            playlist->config.path);
   }
}

/* A playlist file has just been written.  If the cached playlist is a
 * different object reading the same path, its contents are now stale -
 * a scan or a playlist-manager task writing the file the menu happens
 * to have open is the ordinary way this happens.  Drop it rather than
 * let a later reuse serve what is on disk no longer.  When the cached
 * playlist is the one that was written, it is still authoritative; only
 * its size stamp needs to catch up. */
static void playlist_cached_after_write(playlist_t *written)
{
   if (!playlist_cached || !written || !*written->config.path)
      return;
   if (playlist_cached == written)
   {
      playlist_cached->file_size = path_get_size(written->config.path);
      return;
   }
   if (string_is_equal(playlist_cached->config.path, written->config.path))
   {
      /* Mark it, do not free it.
       *
       * This runs from whatever happened to write the file, and the
       * commonest case is the history list: launching an entry pushes
       * it onto g_defaults.content_history, which is a different
       * playlist_t for the same path as the one the menu is displaying
       * from.  Freeing here would pull that object out from under
       * every menu_displaylist caller still holding what
       * playlist_get_cached returned - a use after free whose symptoms
       * appear later and somewhere else entirely.
       *
       * The staleness is what matters, not the memory: flagging it
       * makes the next playlist_init_cached re-read, which is a point
       * where nothing is holding the old pointer. */
      playlist_cached_stale = true;
   }
}

void playlist_write_file(playlist_t *playlist)
{
   size_t i, _len;
   intfstream_t *file = NULL;
   bool compressed    = false;
   bool wrote_ok      = false;
   char write_path[PATH_MAX_LENGTH];

   /* Playlist will be written if any of the
    * following are true:
    * > 'modified' flag is set
    * > Current playlist format (old/new) does not
    *   match requested
    * > Current playlist compression status does
    *   not match requested */
   bool pl_compressed   = ((playlist->flags & CNT_PLAYLIST_FLG_COMPRESSED) > 0);
   bool pl_old_fmt      = ((playlist->flags & CNT_PLAYLIST_FLG_OLD_FMT)    > 0);

   if (   !playlist
       || !*playlist->config.path
       || !( (playlist->flags & CNT_PLAYLIST_FLG_MOD)
#if defined(HAVE_COMPRESSION)
          || (pl_compressed != playlist->config.compress)
#endif
          || (pl_old_fmt    != playlist->config.old_format)
          ))
      return;

   /* Write beside the target and move it into place at the end, rather
    * than truncating the real file and filling it in.  A crash, a power
    * loss or a full disk part way through a write would otherwise leave
    * the user with a truncated playlist - and for a large one that
    * window is tens of milliseconds on every scan and every favourite
    * added.  The temporary sits in the same directory so the move stays
    * within one filesystem. */
   _len = strlcpy(write_path, playlist->config.path, sizeof(write_path));
   if (_len + STRLEN_CONST(".tmp") >= sizeof(write_path))
   {
      RARCH_ERR("[Playlist] Path too long to write safely: \"%s\".\n",
            playlist->config.path);
      return;
   }
   strlcpy_lit(write_path + _len, ".tmp", sizeof(write_path) - _len);

#if defined(HAVE_COMPRESSION)
   if (playlist->config.compress)
      file = intfstream_open_rzip_file(write_path,
            RETRO_VFS_FILE_ACCESS_WRITE);
   else
#endif
      file = intfstream_open_file(write_path,
            RETRO_VFS_FILE_ACCESS_WRITE,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!file)
   {
      RARCH_ERR("[Playlist] Failed to write to file: \"%s\".\n", write_path);
      return;
   }

   /* Get current file compression state */
   compressed = intfstream_is_compressed(file);

#ifdef RARCH_INTERNAL
   if (playlist->config.old_format)
   {
      for (i = 0, _len = RBUF_LEN(playlist->entries); i < _len; i++)
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

      playlist->flags  |=  (CNT_PLAYLIST_FLG_OLD_FMT);
      /* intfstream_printf reports nothing useful per call here, so the
       * old format's success is "we reached the end without bailing" -
       * the same guarantee it gave before, now made explicit because
       * the temporary is only moved into place on success. */
      wrote_ok          = true;
   }
   else
#endif
   {
      rjsonwriter_t* writer = rjsonwriter_open_intfstream(file);
      if (!writer)
      {
         RARCH_ERR("[Playlist] Failed to create JSON writer.\n");
         goto end;
      }
      /*  When compressing playlists, human readability
       *   is not a factor - can skip all indentation
       *   and new line characters */
      if (compressed)
         rjsonwriter_set_options(writer, RJSONWRITER_OPTION_SKIP_WHITESPACE);

      rjsonwriter_raw(writer, "{\n", 2);

      rjsonwriter_add_spaces(writer, 2);
      rjsonwriter_add_string(writer, "version");
      rjsonwriter_raw(writer, ": ", 2);
      rjsonwriter_add_string(writer, "1.5");
      rjsonwriter_raw(writer, ",\n", 2);

      rjsonwriter_add_spaces(writer, 2);
      rjsonwriter_add_string(writer, "default_core_path");
      rjsonwriter_raw(writer, ": ", 2);
      rjsonwriter_add_string(writer, playlist->default_core_path);
      rjsonwriter_raw(writer, ",\n", 2);

      rjsonwriter_add_spaces(writer, 2);
      rjsonwriter_add_string(writer, "default_core_name");
      rjsonwriter_raw(writer, ": ", 2);
      rjsonwriter_add_string(writer, playlist->default_core_name);
      rjsonwriter_raw(writer, ",\n", 2);

      if (playlist->base_content_directory && *playlist->base_content_directory)
      {
         rjsonwriter_add_spaces(writer, 2);
         rjsonwriter_add_string(writer, "base_content_directory");
         rjsonwriter_raw(writer, ": ", 2);
         rjsonwriter_add_string(writer, playlist->base_content_directory);
         rjsonwriter_raw(writer, ",\n", 2);
      }

      rjsonwriter_add_spaces(writer, 2);
      rjsonwriter_add_string(writer, "label_display_mode");
      rjsonwriter_raw(writer, ": ", 2);
      rjsonwriter_rawf(writer, "%d", (int)playlist->label_display_mode);
      rjsonwriter_raw(writer, ",\n", 2);

      rjsonwriter_add_spaces(writer, 2);
      rjsonwriter_add_string(writer, "right_thumbnail_mode");
      rjsonwriter_raw(writer, ": ", 2);
      rjsonwriter_rawf(writer, "%d", (int)playlist->right_thumbnail_mode);
      rjsonwriter_raw(writer, ",\n", 2);

      rjsonwriter_add_spaces(writer, 2);
      rjsonwriter_add_string(writer, "left_thumbnail_mode");
      rjsonwriter_raw(writer, ": ", 2);
      rjsonwriter_rawf(writer, "%d", (int)playlist->left_thumbnail_mode);
      rjsonwriter_raw(writer, ",\n", 2);

      rjsonwriter_add_spaces(writer, 2);
      rjsonwriter_add_string(writer, "thumbnail_match_mode");
      rjsonwriter_raw(writer, ": ", 2);
      rjsonwriter_rawf(writer, "%d", (int)playlist->thumbnail_match_mode);
      rjsonwriter_raw(writer, ",\n", 2);

      rjsonwriter_add_spaces(writer, 2);
      rjsonwriter_add_string(writer, "sort_mode");
      rjsonwriter_raw(writer, ": ", 2);
      rjsonwriter_rawf(writer, "%d", (int)playlist->sort_mode);
      rjsonwriter_raw(writer, ",\n", 2);

      if (playlist->scan_record.content_dir && *playlist->scan_record.content_dir)
      {
         rjsonwriter_add_spaces(writer, 2);
         rjsonwriter_add_string(writer, "scan_content_dir");
         rjsonwriter_raw(writer, ": ", 2);
         rjsonwriter_add_string(writer, playlist->scan_record.content_dir);
         rjsonwriter_raw(writer, ",\n", 2);

         rjsonwriter_add_spaces(writer, 2);
         rjsonwriter_add_string(writer, "scan_file_exts");
         rjsonwriter_raw(writer, ": ", 2);
         rjsonwriter_add_string(writer, playlist->scan_record.file_exts);
         rjsonwriter_raw(writer, ",\n", 2);

         rjsonwriter_add_spaces(writer, 2);
         rjsonwriter_add_string(writer, "scan_dat_file_path");
         rjsonwriter_raw(writer, ": ", 2);
         rjsonwriter_add_string(writer, playlist->scan_record.dat_file_path);
         rjsonwriter_raw(writer, ",\n", 2);

         rjsonwriter_add_spaces(writer, 2);
         rjsonwriter_add_string(writer, "scan_database_name");
         rjsonwriter_raw(writer, ":", 1);
         rjsonwriter_raw(writer, " ", 1);
         rjsonwriter_add_string(writer, playlist->scan_record.database_name);
         rjsonwriter_raw(writer, ",", 1);
         rjsonwriter_raw(writer, "\n", 1);

         rjsonwriter_add_spaces(writer, 2);
         rjsonwriter_add_string(writer, "scan_search_recursively");
         rjsonwriter_raw(writer, ": ", 2);
         {
            bool value = playlist->scan_record.search_recursively;
            rjsonwriter_raw(writer, (value ? "true" : "false"), (value ? 4 : 5));
         }
         rjsonwriter_raw(writer, ",\n", 2);

         rjsonwriter_add_spaces(writer, 2);
         rjsonwriter_add_string(writer, "scan_search_archives");
         rjsonwriter_raw(writer, ": ", 2);
         {
            bool value = playlist->scan_record.search_archives;
            rjsonwriter_raw(writer, (value ? "true" : "false"), (value ? 4 : 5));
         }
         rjsonwriter_raw(writer, ",\n", 2);

         rjsonwriter_add_spaces(writer, 2);
         rjsonwriter_add_string(writer, "scan_filter_dat_content");
         rjsonwriter_raw(writer, ": ", 2);
         {
            bool value = playlist->scan_record.filter_dat_content;
            rjsonwriter_raw(writer, (value ? "true" : "false"), (value ? 4 : 5));
         }
         rjsonwriter_raw(writer, ",\n", 2);

         rjsonwriter_add_spaces(writer, 2);
         rjsonwriter_add_string(writer, "scan_omit_db_ref");
         rjsonwriter_raw(writer, ":", 1);
         rjsonwriter_raw(writer, " ", 1);
         {
            bool value = playlist->scan_record.omit_db_ref;
            rjsonwriter_raw(writer, (value ? "true" : "false"), (value ? 4 : 5));
         }
         rjsonwriter_raw(writer, ",", 1);
         rjsonwriter_raw(writer, "\n", 1);

         rjsonwriter_add_spaces(writer, 2);
         rjsonwriter_add_string(writer, "scan_overwrite_playlist");
         rjsonwriter_raw(writer, ": ", 2);
         {
            bool value = playlist->scan_record.overwrite_playlist;
            rjsonwriter_raw(writer, (value ? "true" : "false"), (value ? 4 : 5));
         }
         rjsonwriter_raw(writer, ",\n", 2);

         rjsonwriter_add_spaces(writer, 2);
         rjsonwriter_add_string(writer, "scan_db_usage");
         rjsonwriter_raw(writer, ":", 1);
         rjsonwriter_raw(writer, " ", 1);
         rjsonwriter_rawf(writer, "%d", (int)playlist->scan_record.db_usage);
         rjsonwriter_raw(writer, ",", 1);
         rjsonwriter_raw(writer, "\n", 1);
      }

      rjsonwriter_add_spaces(writer, 2);
      rjsonwriter_add_string(writer, "items");
      rjsonwriter_raw(writer, ": ", 2);
      rjsonwriter_raw(writer, "[", 1);
      rjsonwriter_raw(writer, "\n", 1);

      for (i = 0, _len = RBUF_LEN(playlist->entries); i < _len; i++)
      {
         rjsonwriter_add_spaces(writer, 4);
         rjsonwriter_raw(writer, "{", 1);

         rjsonwriter_raw(writer, "\n", 1);
         rjsonwriter_add_spaces(writer, 6);
         rjsonwriter_add_string(writer, "path");
         rjsonwriter_raw(writer, ": ", 2);
         rjsonwriter_add_string(writer, playlist->entries[i].path);
         rjsonwriter_raw(writer, ",", 1);

         rjsonwriter_raw(writer, "\n", 1);
         rjsonwriter_add_spaces(writer, 6);
         rjsonwriter_add_string(writer, "label");
         rjsonwriter_raw(writer, ": ", 2);
         rjsonwriter_add_string(writer, playlist->entries[i].label);
         rjsonwriter_raw(writer, ",", 1);

         rjsonwriter_raw(writer, "\n", 1);
         rjsonwriter_add_spaces(writer, 6);
         rjsonwriter_add_string(writer, "core_path");
         rjsonwriter_raw(writer, ": ", 2);
         rjsonwriter_add_string(writer, playlist->entries[i].core_path);
         rjsonwriter_raw(writer, ",", 1);

         rjsonwriter_raw(writer, "\n", 1);
         rjsonwriter_add_spaces(writer, 6);
         rjsonwriter_add_string(writer, "core_name");
         rjsonwriter_raw(writer, ": ", 2);
         rjsonwriter_add_string(writer, playlist->entries[i].core_name);
         rjsonwriter_raw(writer, ",", 1);

         rjsonwriter_raw(writer, "\n", 1);
         rjsonwriter_add_spaces(writer, 6);
         rjsonwriter_add_string(writer, "crc32");
         rjsonwriter_raw(writer, ": ", 2);
         rjsonwriter_add_string(writer, playlist->entries[i].crc32);
         rjsonwriter_raw(writer, ",", 1);

         rjsonwriter_raw(writer, "\n", 1);
         rjsonwriter_add_spaces(writer, 6);
         rjsonwriter_add_string(writer, "db_name");
         rjsonwriter_raw(writer, ": ", 2);
         rjsonwriter_add_string(writer, playlist->entries[i].db_name);

         /* Conditional rows must add "," first */

         /* Typecast required because playlist_entry.entry_slot is unsigned,
          * and 0 and -1 are redundant, but runloop.entry_state_slot is int16_t
          * and must be able to be negative, because 0 is a valid slot */
         if (     (int)playlist->entries[i].entry_slot > 0
               && !strstr(playlist->config.path, FILE_PATH_BUILTIN))
         {
            rjsonwriter_raw(writer, ",", 1);
            rjsonwriter_raw(writer, "\n", 1);
            rjsonwriter_add_spaces(writer, 6);
            rjsonwriter_add_string(writer, "entry_slot");
            rjsonwriter_raw(writer, ": ", 2);
            rjsonwriter_rawf(writer, "%d", (int)playlist->entries[i].entry_slot);
         }

         if (playlist->entries[i].subsystem_ident && *playlist->entries[i].subsystem_ident)
         {
            rjsonwriter_raw(writer, ",", 1);
            rjsonwriter_raw(writer, "\n", 1);
            rjsonwriter_add_spaces(writer, 6);
            rjsonwriter_add_string(writer, "subsystem_ident");
            rjsonwriter_raw(writer, ": ", 2);
            rjsonwriter_add_string(writer, playlist->entries[i].subsystem_ident);
         }

         if (playlist->entries[i].subsystem_name && *playlist->entries[i].subsystem_name)
         {
            rjsonwriter_raw(writer, ",", 1);
            rjsonwriter_raw(writer, "\n", 1);
            rjsonwriter_add_spaces(writer, 6);
            rjsonwriter_add_string(writer, "subsystem_name");
            rjsonwriter_raw(writer, ": ", 2);
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
            rjsonwriter_raw(writer, ": ", 2);
            rjsonwriter_raw(writer, "[", 1);
            rjsonwriter_raw(writer, "\n", 1);

            for (j = 0; j < playlist->entries[i].subsystem_roms->size; j++)
            {
               const struct string_list *roms = playlist->entries[i].subsystem_roms;
               rjsonwriter_add_spaces(writer, 8);
               rjsonwriter_add_string(writer,
                     (roms->elems[j].data && *roms->elems[j].data)
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

         if (i < _len - 1)
            rjsonwriter_raw(writer, ",", 1);

         rjsonwriter_raw(writer, "\n", 1);
      }

      rjsonwriter_add_spaces(writer, 2);
      rjsonwriter_raw(writer, "]\n", 2);
      rjsonwriter_raw(writer, "}\n", 2);

      /* The writer's own failure is the signal that the temporary is
       * incomplete; without it a short write would be moved over a
       * good playlist. */
      if (!rjsonwriter_free(writer))
         RARCH_ERR("[Playlist] Failed to write to file: \"%s\".\n",
               playlist->config.path);
      else
         wrote_ok = true;

      playlist->flags  &= ~(CNT_PLAYLIST_FLG_OLD_FMT);
   }

   playlist->flags     &= ~CNT_PLAYLIST_FLG_MOD;

   if (compressed)
      playlist->flags  |=  (CNT_PLAYLIST_FLG_COMPRESSED);
   else
      playlist->flags  &= ~(CNT_PLAYLIST_FLG_COMPRESSED);

end:
   intfstream_close(file);
   free(file);

   /* Only now does the new content replace the old one.  If anything
    * above failed, the temporary is discarded and the playlist on disk
    * is exactly what it was. */
   if (wrote_ok && filestream_rename(write_path, playlist->config.path) == 0)
   {
      RARCH_LOG("[Playlist] Written to file: \"%s\".\n",
            playlist->config.path);
      playlist_cached_after_write(playlist);
   }
   else
   {
      filestream_delete(write_path);
      RARCH_ERR("[Playlist] Failed to write to file: \"%s\".\n",
            playlist->config.path);
   }
}

/**
 * playlist_free:
 * @playlist            : Playlist handle.
 *
 * Frees playlist handle.
 */
void playlist_free(playlist_t *playlist)
{
   size_t i, _len;

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

   if (playlist->scan_record.database_name)
      free(playlist->scan_record.database_name);
   playlist->scan_record.database_name = NULL;

   if (playlist->entries)
   {
      for (i = 0, _len = RBUF_LEN(playlist->entries); i < _len; i++)
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
 * @playlist           : Playlist handle.
 *
 * Clears all playlist entries in playlist.
 **/
void playlist_clear(playlist_t *playlist)
{
   size_t i, _len;
   if (!playlist)
      return;

   for (i = 0, _len = RBUF_LEN(playlist->entries); i < _len; i++)
   {
      struct playlist_entry *entry = &playlist->entries[i];

      if (entry)
         playlist_free_entry(entry);
   }
   RBUF_CLEAR(playlist->entries);
}

/**
 * playlist_size:
 * @playlist           : Playlist handle.
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
 * @playlist           : Playlist handle.
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

   pCtx->array_depth--;

   if (     (pCtx->flags & JSON_CTX_FLG_IN_ITEMS)
         && (pCtx->array_depth  == 0)
         && (pCtx->object_depth <= 1))
      pCtx->flags &= ~(JSON_CTX_FLG_IN_ITEMS);
   else if ((pCtx->flags & JSON_CTX_FLG_IN_SUBSYSTEM_CONTENT)
         && (pCtx->array_depth  <= 1)
         && (pCtx->object_depth <= 2))
      pCtx->flags &= ~(JSON_CTX_FLG_IN_SUBSYSTEM_CONTENT);

   return true;
}

static bool JSONStartObjectHandler(void *context)
{
   JSONContext *pCtx = (JSONContext *)context;

   pCtx->object_depth++;

   if (     (pCtx->flags & JSON_CTX_FLG_IN_ITEMS)
         && (pCtx->object_depth == 2))
   {
      if (
            (pCtx->array_depth == 1)
         && !(pCtx->flags & JSON_CTX_FLG_CAPACITY_EXCEEDED))
      {
         size_t _len = RBUF_LEN(pCtx->playlist->entries);
         if (_len < pCtx->playlist->config.capacity)
         {
            /* Allocate memory to fit one more item but don't resize the
             * buffer just yet, wait until JSONEndObjectHandler for that */
            if (!RBUF_TRYFIT(pCtx->playlist->entries, _len + 1))
            {
               pCtx->flags |= JSON_CTX_FLG_OOM;
               return false;
            }
            pCtx->current_entry = &pCtx->playlist->entries[_len];
            memset(pCtx->current_entry, 0, sizeof(*pCtx->current_entry));
         }
         else
         {
            /* Hit max item limit.
             * Note: We can't just abort here, since there may
             * be more metadata to read at the end of the file... */
            RARCH_WARN("[Playlist] JSON file contains more entries than current playlist capacity. Excess entries will be discarded.\n");
            pCtx->flags             |= JSON_CTX_FLG_CAPACITY_EXCEEDED;
            pCtx->current_entry      = NULL;
            /* In addition, since we are discarding excess entries,
             * the playlist must be flagged as being modified
             * (i.e. the playlist is not the same as when it was
             * last saved to disk...) */
            pCtx->playlist->flags   |= CNT_PLAYLIST_FLG_MOD;
         }
      }
   }

   return true;
}

static bool JSONEndObjectHandler(void *context)
{
   JSONContext *pCtx = (JSONContext *)context;

   if (     (pCtx->flags & JSON_CTX_FLG_IN_ITEMS)
         && (pCtx->object_depth == 2))
   {
      if (     (pCtx->array_depth == 1)
            && !(pCtx->flags & JSON_CTX_FLG_CAPACITY_EXCEEDED))
         RBUF_RESIZE(pCtx->playlist->entries,
               RBUF_LEN(pCtx->playlist->entries) + 1);
   }

   pCtx->object_depth--;

   return true;
}

static bool JSONStringHandler(void *context, const char *pValue, size_t len)
{
   JSONContext *pCtx = (JSONContext *)context;

   if (     (pCtx->flags & JSON_CTX_FLG_IN_ITEMS)
         && (pCtx->flags & JSON_CTX_FLG_IN_SUBSYSTEM_CONTENT)
         && (pCtx->object_depth == 2)
         && (pCtx->array_depth  == 2))
   {
      if (len && (pValue && *pValue))
      {
         union string_list_elem_attr attr = {0};

         if (!pCtx->current_entry->subsystem_roms)
            pCtx->current_entry->subsystem_roms = string_list_new();

         string_list_append(pCtx->current_entry->subsystem_roms, pValue, attr);
      }
   }
   else if ((pCtx->flags & JSON_CTX_FLG_IN_ITEMS)
         && (pCtx->object_depth == 2))
   {
      if (pCtx->array_depth == 1)
      {
         if (     pCtx->current_string_val
               && len
               && (pValue && *pValue))
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
         if (     pCtx->current_string_val
               && len
               && (pValue && *pValue))
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

static bool JSONNumberHandler(void *context, const char *pValue, size_t len)
{
   JSONContext *pCtx = (JSONContext *)context;

   if (     (pCtx->flags & JSON_CTX_FLG_IN_ITEMS)
         && (pCtx->object_depth == 2))
   {
      if (    (pCtx->array_depth == 1)
            && len
            && (pValue && *pValue))
      {
         if (pCtx->current_entry_uint_val)
            *pCtx->current_entry_uint_val = (unsigned)strtoul(pValue, NULL, 10);
      }
   }
   else if (pCtx->object_depth == 1)
   {
      if (pCtx->array_depth == 0)
      {
         if (len && (pValue && *pValue))
         {
            /* Handle any top-level playlist metadata here */
            if (pCtx->current_meta_label_display_mode_val)
               *pCtx->current_meta_label_display_mode_val   = (enum playlist_label_display_mode)strtoul(pValue, NULL, 10);
            else if (pCtx->current_meta_thumbnail_mode_val)
               *pCtx->current_meta_thumbnail_mode_val       = (enum playlist_thumbnail_mode)strtoul(pValue, NULL, 10);
            else if (pCtx->current_meta_thumbnail_match_mode_val)
               *pCtx->current_meta_thumbnail_match_mode_val = (enum playlist_thumbnail_match_mode)strtoul(pValue, NULL, 10);
            else if (pCtx->current_meta_sort_mode_val)
               *pCtx->current_meta_sort_mode_val            = (enum playlist_sort_mode)strtoul(pValue, NULL, 10);
            else if (pCtx->current_meta_db_usage_val)
               *pCtx->current_meta_db_usage_val             = (enum playlist_sort_mode)strtoul(pValue, NULL, 10);
         }
      }
   }

   pCtx->current_entry_uint_val                = NULL;
   pCtx->current_meta_label_display_mode_val   = NULL;
   pCtx->current_meta_thumbnail_mode_val       = NULL;
   pCtx->current_meta_thumbnail_match_mode_val = NULL;
   pCtx->current_meta_sort_mode_val            = NULL;
   pCtx->current_meta_db_usage_val             = NULL;

   return true;
}

static bool JSONBoolHandler(void *context, bool value)
{
   JSONContext *pCtx = (JSONContext *)context;

   if (   (!(pCtx->flags & JSON_CTX_FLG_IN_ITEMS))
       && (pCtx->object_depth == 1)
       && (pCtx->array_depth  == 0)
       && pCtx->current_meta_bool_val)
      *pCtx->current_meta_bool_val = value;

   pCtx->current_meta_bool_val = NULL;

   return true;
}

static bool JSONObjectMemberHandler(void *context, const char *pValue, size_t len)
{
   JSONContext *pCtx = (JSONContext *)context;

   if (     (pCtx->flags & JSON_CTX_FLG_IN_ITEMS)
         && (pCtx->object_depth == 2))
   {
      if (pCtx->array_depth == 1)
      {
         /* Something went wrong */
         if (pCtx->current_string_val)
            return false;

         if (len && (!(pCtx->flags & JSON_CTX_FLG_CAPACITY_EXCEEDED)))
         {
            pCtx->current_string_val     = NULL;
            pCtx->current_entry_uint_val = NULL;
            pCtx->flags                 &= ~(JSON_CTX_FLG_IN_SUBSYSTEM_CONTENT);
            switch (pValue[0])
            {
               case 'c':
                     if (!strcmp(pValue, "core_name"))
                        pCtx->current_string_val = &pCtx->current_entry->core_name;
                     else if (!strcmp(pValue, "core_path"))
                        pCtx->current_string_val = &pCtx->current_entry->core_path;
                     else if (!strcmp(pValue, "crc32"))
                        pCtx->current_string_val = &pCtx->current_entry->crc32;
                     break;
               case 'd':
                     if (!strcmp(pValue, "db_name"))
                        pCtx->current_string_val = &pCtx->current_entry->db_name;
                     break;
               case 'e':
                     if (!strcmp(pValue, "entry_slot"))
                        pCtx->current_entry_uint_val = &pCtx->current_entry->entry_slot;
                     break;
               case 'l':
                     if (!strcmp(pValue, "label"))
                        pCtx->current_string_val = &pCtx->current_entry->label;
                     else if (!strcmp(pValue, "last_played_day"))
                        pCtx->current_entry_uint_val = &pCtx->current_entry->last_played_day;
                     else if (!strcmp(pValue, "last_played_hour"))
                        pCtx->current_entry_uint_val = &pCtx->current_entry->last_played_hour;
                     else if (!strcmp(pValue, "last_played_minute"))
                        pCtx->current_entry_uint_val = &pCtx->current_entry->last_played_minute;
                     else if (!strcmp(pValue, "last_played_month"))
                        pCtx->current_entry_uint_val = &pCtx->current_entry->last_played_month;
                     else if (!strcmp(pValue, "last_played_second"))
                        pCtx->current_entry_uint_val = &pCtx->current_entry->last_played_second;
                     else if (!strcmp(pValue, "last_played_year"))
                        pCtx->current_entry_uint_val = &pCtx->current_entry->last_played_year;
                     break;
               case 'p':
                     if (!strcmp(pValue, "path"))
                        pCtx->current_string_val = &pCtx->current_entry->path;
                     break;
               case 'r':
                     if (!strcmp(pValue, "runtime_hours"))
                        pCtx->current_entry_uint_val = &pCtx->current_entry->runtime_hours;
                     else if (!strcmp(pValue, "runtime_minutes"))
                        pCtx->current_entry_uint_val = &pCtx->current_entry->runtime_minutes;
                     else if (!strcmp(pValue, "runtime_seconds"))
                        pCtx->current_entry_uint_val = &pCtx->current_entry->runtime_seconds;
                     break;
               case 's':
                     if (!strcmp(pValue, "subsystem_ident"))
                        pCtx->current_string_val = &pCtx->current_entry->subsystem_ident;
                     else if (!strcmp(pValue, "subsystem_name"))
                        pCtx->current_string_val = &pCtx->current_entry->subsystem_name;
                     else if (!strcmp(pValue, "subsystem_roms"))
                        pCtx->flags |= (JSON_CTX_FLG_IN_SUBSYSTEM_CONTENT);
                     break;
            }
         }
      }
   }
   else if ((pCtx->object_depth == 1)
         && (pCtx->array_depth  == 0)
         && len)
   {
      pCtx->current_string_val                    = NULL;
      pCtx->current_meta_label_display_mode_val   = NULL;
      pCtx->current_meta_thumbnail_mode_val       = NULL;
      pCtx->current_meta_thumbnail_match_mode_val = NULL;
      pCtx->current_meta_sort_mode_val            = NULL;
      pCtx->current_meta_db_usage_val             = NULL;
      pCtx->current_meta_bool_val                 = NULL;
      pCtx->flags                                &= ~(JSON_CTX_FLG_IN_ITEMS);

      switch (pValue[0])
      {
         case 'b':
            if (!strcmp(pValue, "base_content_directory"))
               pCtx->current_string_val = &pCtx->playlist->base_content_directory;
            break;
         case 'd':
            if (!strcmp(pValue, "default_core_path"))
               pCtx->current_string_val = &pCtx->playlist->default_core_path;
            else if (!strcmp(pValue, "default_core_name"))
               pCtx->current_string_val = &pCtx->playlist->default_core_name;
            break;
         case 'i':
            if (!strcmp(pValue, "items"))
               pCtx->flags |= JSON_CTX_FLG_IN_ITEMS;
            break;
         case 'l':
            if (!strcmp(pValue, "label_display_mode"))
               pCtx->current_meta_label_display_mode_val = &pCtx->playlist->label_display_mode;
            else if (!strcmp(pValue, "left_thumbnail_mode"))
               pCtx->current_meta_thumbnail_mode_val     = &pCtx->playlist->left_thumbnail_mode;
            break;
         case 'r':
            if (!strcmp(pValue, "right_thumbnail_mode"))
               pCtx->current_meta_thumbnail_mode_val = &pCtx->playlist->right_thumbnail_mode;
            break;
         case 's':
            if (!strcmp(pValue, "scan_content_dir"))
               pCtx->current_string_val         = &pCtx->playlist->scan_record.content_dir;
            else if (!strcmp(pValue, "scan_file_exts"))
               pCtx->current_string_val         = &pCtx->playlist->scan_record.file_exts;
            else if (!strcmp(pValue, "scan_dat_file_path"))
               pCtx->current_string_val         = &pCtx->playlist->scan_record.dat_file_path;
            else if (string_is_equal(pValue, "scan_database_name"))
               pCtx->current_string_val         = &pCtx->playlist->scan_record.database_name;
            else if (!strcmp(pValue, "scan_search_recursively"))
               pCtx->current_meta_bool_val      = &pCtx->playlist->scan_record.search_recursively;
            else if (!strcmp(pValue, "scan_search_archives"))
               pCtx->current_meta_bool_val      = &pCtx->playlist->scan_record.search_archives;
            else if (!strcmp(pValue, "scan_filter_dat_content"))
               pCtx->current_meta_bool_val      = &pCtx->playlist->scan_record.filter_dat_content;
            else if (string_is_equal(pValue, "scan_omit_db_ref"))
               pCtx->current_meta_bool_val      = &pCtx->playlist->scan_record.omit_db_ref;
            else if (string_is_equal(pValue, "scan_db_usage"))
               pCtx->current_meta_db_usage_val  = (unsigned int*)&pCtx->playlist->scan_record.db_usage;
            else if (!strcmp(pValue, "scan_overwrite_playlist"))
               pCtx->current_meta_bool_val      = &pCtx->playlist->scan_record.overwrite_playlist;
            else if (!strcmp(pValue, "sort_mode"))
               pCtx->current_meta_sort_mode_val = &pCtx->playlist->sort_mode;
            break;
         case 't':
            if (!strcmp(pValue, "thumbnail_match_mode"))
               pCtx->current_meta_thumbnail_match_mode_val     = &pCtx->playlist->thumbnail_match_mode;
            break;
      }
   }

   return true;
}

static size_t playlist_get_old_format_metadata_value(
      char *metadata_line, char *s, size_t len)
{
   char *end   = NULL;
   char *start = strchr(metadata_line, '\"');
   if (!start)
      return 0;
   start++;
   if (!(end = strchr(start, '\"')))
      return 0;
   *end        = '\0';
   return strlcpy(s, start, len);
}

/* ------------------------------------------------------------------ */
/* Resumable playlist parse                                            */
/*                                                                     */
/* playlist_init() runs the same machinery to completion, so the      */
/* blocking and budgeted paths cannot drift apart.  The parse state   */
/* is the parser plus the JSONContext (JSON) or the line-group        */
/* buffer (old format); rjson_next() plus the public context queries  */
/* reproduce the rjson_parse() driver exactly (a string at object     */
/* level with an odd context count is a member name; everything else  */
/* dispatches by type), so stopping between events loses nothing.     */
/* ------------------------------------------------------------------ */

/* Budget check cadence: JSON events are cheap, so consult the        */
/* budget once per batch; a power of two so the check is a mask.      */
#define PLAYLIST_PARSE_EVENT_BATCH 256
/* Autofix rewrites paths per entry; cheaper than events, pricier     */
/* than nothing. */
#define PLAYLIST_PARSE_AUTOFIX_BATCH 64

enum playlist_parse_phase
{
   PLAYLIST_PARSE_PHASE_JSON = 0,
   PLAYLIST_PARSE_PHASE_OLD,
   PLAYLIST_PARSE_PHASE_AUTOFIX,
   PLAYLIST_PARSE_PHASE_DONE,
   PLAYLIST_PARSE_PHASE_ERROR
};

struct playlist_parse
{
   playlist_t *playlist;
   intfstream_t *file;
   rjson_t *parser;                       /* JSON phase */
   JSONContext context;                   /* JSON phase */
   char (*line_buf)[PATH_MAX_LENGTH];     /* old-format phase */
   size_t autofix_idx;
   size_t oldref_len;
   size_t newref_len;
   unsigned events;
   enum playlist_parse_phase phase;
   bool res;
   bool autofix_scan_done;
};

static void playlist_parse_close_io(playlist_parse_t *p)
{
   if (p->parser)
   {
      /* A live parser means the JSON phase never finished - an
       * abort (or end after an unfinished step) mid-document.  The
       * same staged-entry hazard the finish path handles applies
       * here: an entry reserved one past the committed length with
       * members already strdup'd into it. */
      if (     p->playlist
            && p->context.current_entry
            && p->context.current_entry ==
                  p->playlist->entries + RBUF_LEN(p->playlist->entries))
      {
         playlist_free_entry(p->context.current_entry);
         p->context.current_entry = NULL;
      }
      rjson_free(p->parser);
      p->parser = NULL;
   }
   if (p->file)
   {
      intfstream_close(p->file);
      free(p->file);
      p->file   = NULL;
   }
   if (p->line_buf)
   {
      free(p->line_buf);
      p->line_buf = NULL;
   }
}

/* Decide what follows the parse: the autofix pass when it applies,   */
/* completion otherwise.  Mirrors the condition playlist_init         */
/* historically evaluated after playlist_read_file returned.          */
static void playlist_parse_enter_autofix(playlist_parse_t *p)
{
   playlist_t *playlist = p->playlist;

   playlist_parse_close_io(p);

   if (!p->res)
   {
      p->phase = PLAYLIST_PARSE_PHASE_ERROR;
      return;
   }

   if (    playlist->config.autofix_paths
       && !string_is_equal(playlist->base_content_directory,
            playlist->config.base_content_directory)
       && playlist->base_content_directory
       && *playlist->base_content_directory)
   {
      p->oldref_len = strlen(playlist->base_content_directory);
      p->newref_len = strlen(playlist->config.base_content_directory);
      p->phase      = PLAYLIST_PARSE_PHASE_AUTOFIX;
      return;
   }

   /* No fixing applies; but when autofix is on the base content
    * directory record must still be refreshed and the file saved -
    * the tail playlist_init always ran. */
   if (    playlist->config.autofix_paths
       && !string_is_equal(playlist->base_content_directory,
            playlist->config.base_content_directory))
   {
      p->phase      = PLAYLIST_PARSE_PHASE_AUTOFIX;
      p->autofix_idx = RBUF_LEN(playlist->entries);   /* skip entries */
      return;
   }

   p->phase = PLAYLIST_PARSE_PHASE_DONE;
}

/* One budgeted slice of the JSON event stream.  This is the          */
/* rjson_parse() driver, transcribed: same handlers, same member      */
/* discrimination, same stop conditions - with a budget consult       */
/* between event batches.                                             */
static int playlist_parse_step_json(playlist_parse_t *p,
      bool (*budget_cb)(void *), void *budget_ud)
{
   playlist_t *playlist   = p->playlist;
   rjson_t *parser        = p->parser;
   JSONContext context;   /* alias for the moved error block below */
   enum rjson_type stop;

   for (;;)
   {
      enum rjson_type t;
      bool ok = true;

      if (     budget_cb
            && ((++p->events & (PLAYLIST_PARSE_EVENT_BATCH - 1)) == 0)
            && !budget_cb(budget_ud))
         return 0;

      t = rjson_next(parser);

      switch (t)
      {
         case RJSON_STRING:
            {
               size_t _len;
               const char *str = rjson_get_string(parser, &_len);
               if (     rjson_get_context_type(parser) == RJSON_OBJECT
                     && (rjson_get_context_count(parser) & 1))
                  ok = JSONObjectMemberHandler(&p->context, str, _len);
               else
                  ok = JSONStringHandler(&p->context, str, _len);
            }
            break;
         case RJSON_NUMBER:
            {
               size_t _len;
               const char *str = rjson_get_string(parser, &_len);
               ok = JSONNumberHandler(&p->context, str, _len);
            }
            break;
         case RJSON_OBJECT:
            ok = JSONStartObjectHandler(&p->context);
            break;
         case RJSON_OBJECT_END:
            ok = JSONEndObjectHandler(&p->context);
            break;
         case RJSON_ARRAY:
            ok = JSONStartArrayHandler(&p->context);
            break;
         case RJSON_ARRAY_END:
            ok = JSONEndArrayHandler(&p->context);
            break;
         case RJSON_TRUE:
            ok = JSONBoolHandler(&p->context, true);
            break;
         case RJSON_FALSE:
            ok = JSONBoolHandler(&p->context, false);
            break;
         case RJSON_NULL:
            /* The driver was given no null handler: a no-op. */
            break;
         case RJSON_ERROR:
         case RJSON_DONE:
         default:
            stop = t;
            goto stopped;
      }

      if (!ok)
      {
         stop = t;
         goto stopped;
      }
   }

stopped:
   context = p->context;   /* the moved block below reads context.flags */
   if (stop != RJSON_DONE)
      {
         if (context.flags & JSON_CTX_FLG_OOM)
         {
            RARCH_WARN("[Playlist] Ran out of memory while parsing JSON playlist.\n");
            p->res = false;
         }
         else
         {
            RARCH_WARN("[Playlist] Error parsing chunk:\n---snip---\n%.*s\n---snip---\n",
                  rjson_get_source_context_len(parser),
                  rjson_get_source_context_buf(parser));
            RARCH_WARN("[Playlist] Error: Invalid JSON at line %d, column %d - %s.\n",
                  (int)rjson_get_source_line(parser),
                  (int)rjson_get_source_column(parser),
                  (*rjson_get_error(parser) ? rjson_get_error(parser) : "format error"));
         }
      }

   /* A parse that stopped inside an items object - malformed
    * input or OOM - leaves an entry staged in the slot one past
    * the committed length: JSONStartObjectHandler only reserves
    * the slot (RBUF_TRYFIT) and the commit (RBUF_RESIZE) happens
    * in JSONEndObjectHandler, which never ran.  playlist_free
    * walks the committed length only, so any members already
    * strdup'd into the staged slot would leak.  Between complete
    * entries current_entry legitimately points at the LAST
    * committed slot, so only the one-past position identifies a
    * staged entry. */
   if (     p->context.current_entry
         && p->context.current_entry ==
               playlist->entries + RBUF_LEN(playlist->entries))
      playlist_free_entry(p->context.current_entry);

   playlist_parse_enter_autofix(p);
   return (p->phase == PLAYLIST_PARSE_PHASE_ERROR) ? -1 : 1;
}

/* One budgeted slice of the old six-line format: a group per         */
/* iteration, budget between groups.                                  */
static int playlist_parse_step_old(playlist_parse_t *p,
      bool (*budget_cb)(void *), void *budget_ud)
{
   playlist_t *playlist              = p->playlist;
   intfstream_t *file                = p->file;
   char (*line_buf)[PATH_MAX_LENGTH] = p->line_buf;

   for (;;)
   {
      size_t i;
      size_t lines_read = 0;
      size_t _len       = RBUF_LEN(playlist->entries);

      if (budget_cb && !budget_cb(budget_ud))
         return 0;

      if (_len >= playlist->config.capacity)
         break;

      /* Attempt to read the next 'PLAYLIST_ENTRIES'
       * lines from the file */
      for (i = 0; i < PLAYLIST_ENTRIES; i++)
      {
         *line_buf[i] = '\0';

         if (!intfstream_gets(file, line_buf[i], sizeof(line_buf[i])))
            break;
         /* Ensure line is NULL terminated, regardless of
          * Windows or Unix line endings */
         string_replace_all_chars(line_buf[i], '\r', '\0');
         string_replace_all_chars(line_buf[i], '\n', '\0');

         lines_read++;
      }

      /* If a 'full set' of lines were read, then this
       * is a valid playlist entry */
      if (lines_read >= PLAYLIST_ENTRIES)
      {
         struct playlist_entry* entry;

         if (!RBUF_TRYFIT(playlist->entries, _len + 1))
         {
            p->res = false; /* out of memory */
            break;
         }
         RBUF_RESIZE(playlist->entries, _len + 1);
         entry = &playlist->entries[_len];

         memset(entry, 0, sizeof(*entry));

         /* Path */
         if (*line_buf[0])
            entry->path      = strdup(line_buf[0]);
         /* Label */
         if (*line_buf[1])
            entry->label     = strdup(line_buf[1]);
         /* Core_path */
         if (*line_buf[2])
            entry->core_path = strdup(line_buf[2]);
         /* Core_name */
         if (*line_buf[3])
            entry->core_name = strdup(line_buf[3]);
         /* CRC32 */
         if (*line_buf[4])
            entry->crc32     = strdup(line_buf[4]);
         /* db_name */
         if (*line_buf[5])
            entry->db_name   = strdup(line_buf[5]);
         continue;
      }
      /* If fewer than 'PLAYLIST_ENTRIES' lines were
       * read, then this is metadata */
      {
            char default_core_path[PATH_MAX_LENGTH];
            char default_core_name[NAME_MAX_LENGTH];

            default_core_path[0] = '\0';
            default_core_name[0] = '\0';

            /* Get default_core_path */
            if (lines_read < 1)
               goto meta_done;

            if (strncmp("default_core_path",
                     line_buf[0],
                     STRLEN_CONST("default_core_path")) == 0)
               playlist_get_old_format_metadata_value(
                     line_buf[0], default_core_path, sizeof(default_core_path));

            /* Get default_core_name */
            if (lines_read < 2)
               goto meta_done;

            if (strncmp("default_core_name",
                     line_buf[1],
                     STRLEN_CONST("default_core_name")) == 0)
               playlist_get_old_format_metadata_value(
                     line_buf[1], default_core_name, sizeof(default_core_name));

            /* > Populate default core path/name, if required
             *   (if one is empty, the other should be ignored) */
            if (   *default_core_path
                && *default_core_name)
            {
               playlist->default_core_path = strdup(default_core_path);
               playlist->default_core_name = strdup(default_core_name);
            }

            /* Get label_display_mode */
            if (lines_read < 3)
               goto meta_done;

            if (strncmp("label_display_mode",
                     line_buf[2],
                     STRLEN_CONST("label_display_mode")) == 0)
            {
               char display_mode_str[4];
               if (playlist_get_old_format_metadata_value(
                     line_buf[2], display_mode_str, sizeof(display_mode_str)) > 0)
               {
                  unsigned display_mode = string_to_unsigned(display_mode_str);
                  if (display_mode <= LABEL_DISPLAY_MODE_KEEP_REGION_AND_DISC_INDEX)
                     playlist->label_display_mode = (enum playlist_label_display_mode)display_mode;
               }
            }

            /* Get thumbnail modes */
            if (lines_read < 4)
               goto meta_done;

            if (strncmp("thumbnail_mode",
                     line_buf[3],
                     STRLEN_CONST("thumbnail_mode")) == 0)
            {
               char thumbnail_mode_str[8];
               if (playlist_get_old_format_metadata_value(
                        line_buf[3], thumbnail_mode_str,
                        sizeof(thumbnail_mode_str)) > 0)
               {
                  char *delim = strchr(thumbnail_mode_str, '|');
                  if (delim)
                  {
                     unsigned thumbnail_mode;
                     *delim = '\0';
                     /* Right thumbnail mode */
                     thumbnail_mode = string_to_unsigned(thumbnail_mode_str);
                     if (thumbnail_mode <= PLAYLIST_THUMBNAIL_MODE_LOGOS)
                        playlist->right_thumbnail_mode = (enum playlist_thumbnail_mode)thumbnail_mode;
                     /* Left thumbnail mode */
                     thumbnail_mode = string_to_unsigned(delim + 1);
                     if (thumbnail_mode <= PLAYLIST_THUMBNAIL_MODE_LOGOS)
                        playlist->left_thumbnail_mode = (enum playlist_thumbnail_mode)thumbnail_mode;
                  }
               }
            }

            /* Get sort_mode */
            if (lines_read < 5)
               goto meta_done;

            if (strncmp("sort_mode",
                     line_buf[4],
                     STRLEN_CONST("sort_mode")) == 0)
            {
               char sort_mode_str[4];
               if (playlist_get_old_format_metadata_value(
                     line_buf[4], sort_mode_str, sizeof(sort_mode_str)) > 0)
               {
                  unsigned sort_mode = string_to_unsigned(sort_mode_str);
                  if (sort_mode <= PLAYLIST_SORT_MODE_OFF)
                     playlist->sort_mode = (enum playlist_sort_mode)sort_mode;
               }
            }

meta_done: ;
      }
      break;
   }

   playlist_parse_enter_autofix(p);
   return (p->phase == PLAYLIST_PARSE_PHASE_ERROR) ? -1 : 1;
}

/* The autofix pass playlist_init historically ran after reading:     */
/* per-entry path rewriting when the recorded base content directory  */
/* differs from the configured one, then the scan-record fixups and   */
/* the refreshed record + save.                                       */
static int playlist_parse_step_autofix(playlist_parse_t *p,
      bool (*budget_cb)(void *), void *budget_ud)
{
   playlist_t *playlist = p->playlist;
   size_t count         = RBUF_LEN(playlist->entries);
   char tmp_entry_path[PATH_MAX_LENGTH];

   if (     playlist->base_content_directory
         && *playlist->base_content_directory)
   {
      for (; p->autofix_idx < count; p->autofix_idx++)
      {
         size_t j;
         struct playlist_entry* entry = &playlist->entries[p->autofix_idx];

         if (     budget_cb
               && ((p->autofix_idx & (PLAYLIST_PARSE_AUTOFIX_BATCH - 1)) == 0)
               && !budget_cb(budget_ud))
            return 0;

         if (!entry || (!entry->path || !*entry->path))
            continue;

         /* Fix entry path */
         tmp_entry_path[0] = '\0';
         path_replace_base_path_and_convert_to_local_file_system(
               tmp_entry_path, entry->path,
               playlist->base_content_directory, p->oldref_len,
               playlist->config.base_content_directory, p->newref_len,
               sizeof(tmp_entry_path));

         free(entry->path);
         entry->path = strdup(tmp_entry_path);

         /* Fix subsystem roms paths*/
         if (     (entry->subsystem_roms)
               && (entry->subsystem_roms->size > 0))
         {
            struct string_list* subsystem_roms_new_paths = string_list_new();
            union string_list_elem_attr attributes       = { 0 };

            if (!subsystem_roms_new_paths)
            {
               p->res   = false;
               p->phase = PLAYLIST_PARSE_PHASE_ERROR;
               return -1;
            }

            for (j = 0; j < entry->subsystem_roms->size; j++)
            {
               const char* subsystem_rom_path = entry->subsystem_roms->elems[j].data;

               if (!subsystem_rom_path || !*subsystem_rom_path)
                  continue;

               tmp_entry_path[0] = '\0';
               path_replace_base_path_and_convert_to_local_file_system(
                     tmp_entry_path,
                     subsystem_rom_path,
                     playlist->base_content_directory, p->oldref_len,
                     playlist->config.base_content_directory, p->newref_len,
                     sizeof(tmp_entry_path));
               string_list_append(subsystem_roms_new_paths, tmp_entry_path, attributes);
            }

            string_list_free(entry->subsystem_roms);
            entry->subsystem_roms = subsystem_roms_new_paths;
         }
      }

      if (!p->autofix_scan_done)
      {
         p->autofix_scan_done = true;

         /* Fix scan record content directory */
         if (playlist->scan_record.content_dir && *playlist->scan_record.content_dir)
         {
            tmp_entry_path[0] = '\0';
            path_replace_base_path_and_convert_to_local_file_system(
                  tmp_entry_path,
                  playlist->scan_record.content_dir,
                  playlist->base_content_directory, p->oldref_len,
                  playlist->config.base_content_directory, p->newref_len,
                  sizeof(tmp_entry_path));

            free(playlist->scan_record.content_dir);
            playlist->scan_record.content_dir = strdup(tmp_entry_path);
         }

         /* Fix scan record arcade DAT file */
         if (playlist->scan_record.dat_file_path && *playlist->scan_record.dat_file_path)
         {
            tmp_entry_path[0] = '\0';
            path_replace_base_path_and_convert_to_local_file_system(
                  tmp_entry_path,
                  playlist->scan_record.dat_file_path,
                  playlist->base_content_directory, p->oldref_len,
                  playlist->config.base_content_directory, p->newref_len,
                  sizeof(tmp_entry_path));

            free(playlist->scan_record.dat_file_path);
            playlist->scan_record.dat_file_path = strdup(tmp_entry_path);
         }
      }
   }

   /* Update playlist base content directory*/
   if (playlist->base_content_directory)
      free(playlist->base_content_directory);
   playlist->base_content_directory = strdup(playlist->config.base_content_directory);

   /* Save playlist */
   playlist->flags   |=  CNT_PLAYLIST_FLG_MOD;
   playlist_write_file(playlist);

   /* The write changed the file this stamp describes - same hazard
    * as the format conversion in playlist_init_cached_install, and
    * playlist_cached_after_write() cannot refresh a playlist not
    * yet installed.  Left stale, the reuse check never matches. */
   playlist->file_size = path_get_size(playlist->config.path);

   p->phase = PLAYLIST_PARSE_PHASE_DONE;
   return 1;
}

playlist_parse_t *playlist_parse_begin(const playlist_config_t *config)
{
   playlist_parse_t *p  = NULL;
   playlist_t *playlist = (playlist_t*)malloc(sizeof(*playlist));
   int test_char;

   if (!playlist)
      return NULL;

   /* Set initial values */
   playlist->flags                          = 0;
   playlist->default_core_name              = NULL;
   playlist->default_core_path              = NULL;
   playlist->base_content_directory         = NULL;
   playlist->entries                        = NULL;
   playlist->label_display_mode             = LABEL_DISPLAY_MODE_DEFAULT;
   playlist->right_thumbnail_mode           = PLAYLIST_THUMBNAIL_MODE_DEFAULT;
   playlist->left_thumbnail_mode            = PLAYLIST_THUMBNAIL_MODE_DEFAULT;
   playlist->thumbnail_match_mode           = PLAYLIST_THUMBNAIL_MATCH_MODE_DEFAULT;
   playlist->sort_mode                      = PLAYLIST_SORT_MODE_DEFAULT;

   playlist->scan_record.search_recursively = false;
   playlist->scan_record.search_archives    = false;
   playlist->scan_record.filter_dat_content = false;
   playlist->scan_record.overwrite_playlist = false;
   playlist->scan_record.omit_db_ref        = false;
   playlist->scan_record.content_dir        = NULL;
   playlist->scan_record.file_exts          = NULL;
   playlist->scan_record.dat_file_path      = NULL;
   playlist->scan_record.database_name      = NULL;
   playlist->scan_record.db_usage           = 4; /*MANUAL_CONTENT_SCAN_USE_DB_NONE*/

   /* Cache configuration parameters */
   if (!playlist_config_copy(config, &playlist->config))
      goto error;
   if (!(p = (playlist_parse_t*)calloc(1, sizeof(*p))))
      goto error;

   p->playlist = playlist;
   p->res      = true;
   p->phase    = PLAYLIST_PARSE_PHASE_DONE;   /* until proven otherwise */

#if defined(HAVE_COMPRESSION)
      /* Always use RZIP interface when reading playlists
       * > this will automatically handle uncompressed
       *   data */
   p->file = intfstream_open_rzip_file(
         playlist->config.path,
         RETRO_VFS_FILE_ACCESS_READ);
#else
   p->file = intfstream_open_file(
         playlist->config.path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
#endif

   /* If playlist file does not exist,
    * create an empty playlist instead */
   if (!p->file)
   {
      /* Records -1, exactly as the stat did for a missing file, so
       * a later reuse check compares like with like. */
      playlist->file_size = path_get_size(playlist->config.path);
      playlist_parse_enter_autofix(p);
      return p;
   }

   if (intfstream_is_compressed(p->file))
   {
      playlist->flags    |=  CNT_PLAYLIST_FLG_COMPRESSED;
      /* The size stamp must be the ON-DISK size: it is compared
       * against path_get_size() to decide whether a cached playlist
       * is still current.  A compressed stream reports the
       * UNCOMPRESSED size from its rzip header - a different number
       * entirely - so this case still pays the stat. */
      playlist->file_size = path_get_size(playlist->config.path);
   }
   else
   {
      playlist->flags    &= ~CNT_PLAYLIST_FLG_COMPRESSED;
      /* Uncompressed: the stream is the file, and its size is the
       * on-disk size the reuse check wants.  Taking it from the
       * handle already open saves a stat per playlist read - one
       * filesystem round trip on the network VFS backends, on a
       * path the menu takes for every playlist it has not cached. */
      playlist->file_size = intfstream_get_size(p->file);
   }

   /* Detect format of playlist
    * > Read file until we find the first printable
    *   non-whitespace ASCII character */
   do
   {
      /* Read error or EOF (end of file) */
      if ((test_char = intfstream_getc(p->file)) == EOF)
      {
         /* Empty file: an empty playlist, successfully. */
         playlist_parse_enter_autofix(p);
         return p;
      }
   }while(!isgraph(test_char) || test_char > 0x7F);

   if (test_char != '{')
      playlist->flags |=  (CNT_PLAYLIST_FLG_OLD_FMT);
   else
      playlist->flags &= ~(CNT_PLAYLIST_FLG_OLD_FMT);

   /* Reset file to start */
   intfstream_rewind(p->file);

   if (!(playlist->flags & CNT_PLAYLIST_FLG_OLD_FMT))
   {
      p->context.playlist = playlist;

      if (!(p->parser = rjson_open_intfstream(p->file)))
      {
         RARCH_ERR("[Playlist] Failed to create JSON parser.\n");
         playlist_parse_enter_autofix(p);
         return p;
      }

      rjson_set_options(p->parser,
              RJSON_OPTION_ALLOW_UTF8BOM
            | RJSON_OPTION_ALLOW_COMMENTS
            | RJSON_OPTION_ALLOW_UNESCAPED_CONTROL_CHARACTERS
            | RJSON_OPTION_REPLACE_INVALID_ENCODING);

      p->phase = PLAYLIST_PARSE_PHASE_JSON;
      return p;
   }

   /* Heap-held: at six entries this is 3 KiB even at the console
    * path length, on the playlist load path that runs from task
    * threads.  On allocation failure the legacy-format file simply
    * reads as empty, matching every other read failure here. */
   if (!(p->line_buf = (char (*)[PATH_MAX_LENGTH])
         calloc(PLAYLIST_ENTRIES, PATH_MAX_LENGTH)))
   {
      playlist_parse_enter_autofix(p);
      return p;
   }

   p->phase = PLAYLIST_PARSE_PHASE_OLD;
   return p;

error:
   playlist_free(playlist);
   return NULL;
}

int playlist_parse_step(playlist_parse_t *p,
      bool (*budget_cb)(void *), void *budget_ud)
{
   if (!p)
      return -1;

   for (;;)
   {
      int r;
      switch (p->phase)
      {
         case PLAYLIST_PARSE_PHASE_JSON:
            r = playlist_parse_step_json(p, budget_cb, budget_ud);
            break;
         case PLAYLIST_PARSE_PHASE_OLD:
            r = playlist_parse_step_old(p, budget_cb, budget_ud);
            break;
         case PLAYLIST_PARSE_PHASE_AUTOFIX:
            r = playlist_parse_step_autofix(p, budget_cb, budget_ud);
            break;
         case PLAYLIST_PARSE_PHASE_DONE:
            return 1;
         case PLAYLIST_PARSE_PHASE_ERROR:
         default:
            return -1;
      }
      if (r <= 0)
         return r;
      /* Phase completed; the next phase may still fit the budget -
       * loop and let its own budget consults decide. */
      if (p->phase == PLAYLIST_PARSE_PHASE_DONE)
         return 1;
      if (p->phase == PLAYLIST_PARSE_PHASE_ERROR)
         return -1;
   }
}

playlist_t *playlist_parse_end(playlist_parse_t *p)
{
   playlist_t *playlist = NULL;

   if (!p)
      return NULL;

   playlist_parse_close_io(p);

   if (p->phase == PLAYLIST_PARSE_PHASE_DONE)
   {
      playlist = p->playlist;
      p->playlist = NULL;
   }
   else if (p->playlist)
      playlist_free(p->playlist);

   free(p);
   return playlist;
}

void playlist_parse_abort(playlist_parse_t *p)
{
   if (!p)
      return;
   playlist_parse_close_io(p);
   if (p->playlist)
      playlist_free(p->playlist);
   free(p);
}


void playlist_free_cached(void)
{
   if (playlist_cached && !(playlist_cached->flags & CNT_PLAYLIST_FLG_CACHED_EXT))
      playlist_free(playlist_cached);
   playlist_cached       = NULL;
   playlist_cached_stale = false;
}

playlist_t *playlist_get_cached(void)
{
   if (playlist_cached)
      return playlist_cached;
   return NULL;
}

/* May the cached playlist stand in for the one being asked for?
 *
 * Reading a playlist is not cheap - a large one is tens of milliseconds
 * of JSON parsing and six allocations per entry - and the menu asks for
 * the same file every time it rebuilds a display list, so answering
 * from the cache is the difference between paying that once and paying
 * it on every navigation.
 *
 * The bar for reuse is deliberately high, because the previous
 * behaviour of always re-reading was never wrong. Everything the parse
 * depends on has to match: the file, and every config field that
 * changes how it is read or what is kept from it.  Anything modified
 * inside this process invalidates the cache where it happens, so this
 * only has to catch changes made behind our back - for which the file
 * size is the one signal the VFS layer offers portably. */
static bool playlist_cached_is_reusable(const playlist_config_t *config)
{
   if (!playlist_cached || playlist_cached_stale)
      return false;
   if (!string_is_equal(playlist_cached->config.path, config->path))
      return false;
   if (     playlist_cached->config.capacity            != config->capacity
         || playlist_cached->config.old_format          != config->old_format
         || playlist_cached->config.compress            != config->compress
         || playlist_cached->config.fuzzy_archive_match != config->fuzzy_archive_match
         || playlist_cached->config.autofix_paths       != config->autofix_paths)
      return false;
   /* Only compare the recorded base when autofix could act on it
    * (playlist_parse_enter_autofix's own gate).  With autofix off a
    * fresh parse leaves the record alone, so the cache is exactly
    * what a re-read would produce - comparing unconditionally
    * rejected such a playlist forever (issue #19427: every rebuild
    * freed the playlist just installed and restarted the read). */
   if (     config->autofix_paths
         && !string_is_equal(playlist_cached->base_content_directory
            ? playlist_cached->base_content_directory : "",
            config->base_content_directory))
      return false;
   /* Changed underneath us since it was read. */
   if (playlist_cached->file_size != path_get_size(config->path))
      return false;
   return true;
}

/* Shared tail of the cached-init paths: sync the on-disk
 * format/compression with the requested settings and install the
 * playlist as the cache.  Main-thread only, like every other
 * mutation of playlist_cached. */
static void playlist_init_cached_install(playlist_t *playlist)
{
   bool pl_compressed = ((playlist->flags & CNT_PLAYLIST_FLG_COMPRESSED) > 0);
   bool pl_old_fmt    = ((playlist->flags & CNT_PLAYLIST_FLG_OLD_FMT)    > 0);
   /* If playlist format/compression state
    * does not match requested settings, update
    * file on disk immediately */
   if (
#if defined(HAVE_COMPRESSION)
       (pl_compressed != playlist->config.compress) ||
#endif
       (pl_old_fmt != playlist->config.old_format))
   {
      playlist_write_file(playlist);
      /* The rewrite changed the file this playlist records the size
       * of - a format or compression conversion changes it a lot -
       * so the stamp taken at read time now describes a file that no
       * longer exists.  Left stale, playlist_cached_is_reusable()
       * compares it against the new on-disk size, never matches, and
       * every subsequent request re-reads and re-parses the same
       * playlist for as long as the on-disk format disagrees with
       * the settings.  playlist_write_file() cannot refresh it
       * itself here: it defers to playlist_cached_after_write(),
       * which only knows how to update the playlist that is already
       * installed as the cache, and this one is installed below. */
      playlist->file_size = path_get_size(playlist->config.path);
   }

   playlist_free_cached();
   playlist_cached       = playlist;
   playlist_cached_stale = false;
}

bool playlist_init_cached(const playlist_config_t *config)
{
   playlist_t *playlist;

   if (playlist_cached_is_reusable(config))
      return true;

   playlist_free_cached();

   if (!(playlist = playlist_init(config)))
      return false;

   playlist_init_cached_install(playlist);
   return true;
}

/* ------------------------------------------------------------------ */
/* Deferred cached init: the same contract as playlist_init_cached,   */
/* spread over budgeted steps.  One pending parse at a time (the      */
/* menu asks for one playlist at a time); a request for a different   */
/* playlist abandons the previous pending parse.                      */
/*                                                                    */
/* Threading discipline mirrors menu_dirwalk: _deferred and _finish   */
/* run on the main thread (they touch the global cache);              */
/* _continue only advances the private parse handle, so a worker      */
/* task may drive it, with the install handed back to the main        */
/* thread through _finish.                                            */
/* ------------------------------------------------------------------ */

static playlist_parse_t   *playlist_cached_pending        = NULL;
static playlist_config_t   playlist_cached_pending_config;

static bool playlist_config_matches(const playlist_config_t *a,
      const playlist_config_t *b)
{
   return  string_is_equal(a->path, b->path)
        && string_is_equal(a->base_content_directory, b->base_content_directory)
        && (a->capacity            == b->capacity)
        && (a->old_format          == b->old_format)
        && (a->compress            == b->compress)
        && (a->fuzzy_archive_match == b->fuzzy_archive_match)
        && (a->autofix_paths       == b->autofix_paths);
}

/* True while a deferred cached init is part way through a read. */
bool playlist_init_cached_pending(void)
{
   return playlist_cached_pending != NULL;
}

void playlist_init_cached_defer_abort(void)
{
   if (playlist_cached_pending)
   {
      playlist_parse_abort(playlist_cached_pending);
      playlist_cached_pending = NULL;
   }
}

int playlist_init_cached_continue(bool (*budget_cb)(void *), void *budget_ud)
{
   int r;
   if (!playlist_cached_pending)
      return -1;
   if ((r = playlist_parse_step(playlist_cached_pending,
         budget_cb, budget_ud)) == 0)
      return 0;
   if (r < 0)
   {
      playlist_init_cached_defer_abort();
      return -1;
   }
   /* Parse complete; the playlist is not yet installed - that step
    * belongs to playlist_init_cached_finish() on the main thread. */
   return 1;
}

int playlist_init_cached_finish(void)
{
   playlist_t *playlist;
   if (!playlist_cached_pending)
      return -1;
   playlist                = playlist_parse_end(playlist_cached_pending);
   playlist_cached_pending = NULL;
   if (!playlist)
      return -1;
   playlist_init_cached_install(playlist);
   return 1;
}

int playlist_init_cached_deferred(const playlist_config_t *config,
      bool (*budget_cb)(void *), void *budget_ud)
{
   int r;

   if (playlist_cached_is_reusable(config))
   {
      /* A pending parse for anything is now moot. */
      playlist_init_cached_defer_abort();
      return 1;
   }

   /* A pending parse for a different playlist is superseded. */
   if (     playlist_cached_pending
         && !playlist_config_matches(&playlist_cached_pending_config, config))
      playlist_init_cached_defer_abort();

   if (!playlist_cached_pending)
   {
      if (!(playlist_cached_pending = playlist_parse_begin(config)))
         return -1;
      playlist_config_copy(config, &playlist_cached_pending_config);

      /* Drop the cached playlist the moment a DIFFERENT one is
       * requested, rather than when the new one is ready.
       *
       * The blocking playlist_init_cached() replaces the cache in a
       * single call, so there is no window in which a caller can see
       * the wrong playlist.  This one can yield, and a caller that
       * reads playlist_get_cached() during that window - the menu
       * does, to build its entry list - would be handed the playlist
       * it was looking at BEFORE, and would render those entries
       * under the new playlist's heading.  That is not a cosmetic
       * mismatch: selecting an entry would launch content from the
       * wrong system.
       *
       * Freeing here costs nothing that is not already being paid:
       * we only reach this point because the cache could not be
       * reused, so it was going to be replaced regardless.  Callers
       * see NULL until the parse completes, which they already
       * handle - it is the same thing they see before any playlist
       * has been loaded. */
      playlist_free_cached();
   }

   if ((r = playlist_init_cached_continue(budget_cb, budget_ud)) == 0)
      return 0;
   if (r < 0)
      return -1;
   return playlist_init_cached_finish();
}

/**
 * playlist_init:
 * @config            : Playlist configuration object.
 *
 * Creates and initializes a playlist.  Runs the resumable parse
 * machinery to completion, so this and the budgeted path share one
 * implementation.
 *
 * Returns: handle to new playlist if successful, otherwise NULL
 **/
playlist_t *playlist_init(const playlist_config_t *config)
{
   playlist_parse_t *p = playlist_parse_begin(config);
   if (!p)
      return NULL;
   playlist_parse_step(p, NULL, NULL);
   return playlist_parse_end(p);
}

static int playlist_qsort_func(const void *a_ptr,
      const void *b_ptr)
{
   const struct playlist_entry *a = (const struct playlist_entry *)a_ptr;
   const struct playlist_entry *b = (const struct playlist_entry *)b_ptr;
   char *a_str                    = NULL;
   char *b_str                    = NULL;
   char a_fallback_label[NAME_MAX_LENGTH];
   char b_fallback_label[NAME_MAX_LENGTH];

   if (!a || !b)
      return 0;

   a_str                  = a->label;
   b_str                  = b->label;

   /* It is quite possible for playlist labels
    * to be blank. If that is the case, have to use
    * filename as a fallback (this is slow, but we
    * have no other option...) */
   if (!a_str || !*a_str)
   {
      a_fallback_label[0] = '\0';

      if (a->path && *a->path)
         fill_pathname(a_fallback_label,
               path_basename_nocompression(a->path),
               "",
               sizeof(a_fallback_label));
      /* If filename is also empty, use core name
       * instead -> this matches the behaviour of
       * menu_displaylist_parse_playlist() */
      else if (a->core_name && *a->core_name)
         strlcpy(a_fallback_label, a->core_name, sizeof(a_fallback_label));

      /* If both filename and core name are empty,
       * then have to compare an empty string
       * -> again, this is to match the behaviour of
       * menu_displaylist_parse_playlist() */

      a_str = a_fallback_label;
   }

   if (!b_str || !*b_str)
   {
      b_fallback_label[0] = '\0';

      if (b->path && *b->path)
         fill_pathname(b_fallback_label,
               path_basename_nocompression(b->path),
               "",
               sizeof(b_fallback_label));
      else if (b->core_name && *b->core_name)
         strlcpy(b_fallback_label, b->core_name, sizeof(b_fallback_label));

      b_str = b_fallback_label;
   }

   return strcasecmp(a_str, b_str);
}

void playlist_qsort(playlist_t *playlist)
{
   /* Avoid inadvertent sorting if 'sort mode'
    * has been set explicitly to PLAYLIST_SORT_MODE_OFF */
   if (   !playlist
       || !playlist->entries
       || (playlist->sort_mode == PLAYLIST_SORT_MODE_OFF))
      return;

   qsort(playlist->entries, RBUF_LEN(playlist->entries),
         sizeof(struct playlist_entry),
         playlist_qsort_func);
}

void command_playlist_push_write(
      playlist_t *playlist,
      const struct playlist_entry *entry)
{
   if (playlist && playlist_push(playlist, entry))
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
   if (!playlist || idx >= RBUF_LEN(playlist->entries))
      return false;
   return    playlist_path_equal(path, playlist->entries[idx].path, &playlist->config)
          && string_is_equal(path_basename_nocompression(playlist->entries[idx].core_path),
                path_basename_nocompression(core_path));
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

   if (   (!entry_a->path || !*entry_a->path)
       && (!entry_a->core_path || !*entry_a->core_path)
       && (!entry_b->path || !*entry_b->path)
       && (!entry_b->core_path || !*entry_b->core_path))
      return true;

   /* Check content paths */
   if (entry_a->path && *entry_a->path)
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
   if (entry_a->core_path && *entry_a->core_path)
   {
      strlcpy(real_core_path_a, entry_a->core_path, sizeof(real_core_path_a));
      if (   !string_is_equal(real_core_path_a, FILE_PATH_DETECT)
          && !string_is_equal(real_core_path_a, FILE_PATH_BUILTIN))
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
   size_t _len;
   struct playlist_entry *entry_a = NULL;
   struct playlist_entry *entry_b = NULL;

   if (!playlist)
      return false;

   _len = RBUF_LEN(playlist->entries);

   if ((idx_a >= _len) || (idx_b >= _len))
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

   /* Check content paths match */
   if (!playlist_path_matches_entry(
         entry_a->path_id, entry_b, &playlist->config))
      return false;

   /* Check core paths match */
   {
      char real_core_path_a[PATH_MAX_LENGTH];

      if (entry_a->core_path && *entry_a->core_path)
      {
         strlcpy(real_core_path_a, entry_a->core_path,
               sizeof(real_core_path_a));
         if (   !string_is_equal(real_core_path_a, FILE_PATH_DETECT)
             && !string_is_equal(real_core_path_a, FILE_PATH_BUILTIN))
            playlist_resolve_path(PLAYLIST_SAVE, true,
                  real_core_path_a, sizeof(real_core_path_a));
      }
      else
         real_core_path_a[0] = '\0';

      return playlist_core_path_equal(
            real_core_path_a, entry_b->core_path, &playlist->config);
   }
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
   if (!playlist || !db_name || idx >= RBUF_LEN(playlist->entries))
      return;

   if (playlist->entries[idx].db_name && *playlist->entries[idx].db_name)
       *db_name = playlist->entries[idx].db_name;
   else
   {
       const char *conf_path_basename = path_basename_nocompression(playlist->config.path);

       /* Only use file basename if this is a 'collection' playlist
        * (i.e. ignore history/favourites) */
       if (
              (conf_path_basename && *conf_path_basename)
           && !string_is_equal(conf_path_basename, FILE_PATH_CONTENT_HISTORY)
           && !string_is_equal(conf_path_basename, FILE_PATH_CONTENT_FAVORITES)
           )
           *db_name = conf_path_basename;
       else
       {
          core_info_t *core_info = playlist_entry_get_core_info(&playlist->entries[idx]);
          if (core_info && core_info->databases)
             *db_name = core_info->databases;
       }
   }
}

const char *playlist_get_default_core_path(playlist_t *playlist)
{
   return playlist ? playlist->default_core_path : NULL;
}

const char *playlist_get_default_core_name(playlist_t *playlist)
{
   return playlist ? playlist->default_core_name : NULL;
}

enum playlist_label_display_mode playlist_get_label_display_mode(playlist_t *playlist)
{
   return playlist ? playlist->label_display_mode : LABEL_DISPLAY_MODE_DEFAULT;
}

enum playlist_thumbnail_mode playlist_get_thumbnail_mode(
      playlist_t *playlist, enum playlist_thumbnail_id thumbnail_id)
{
   if (playlist)
   {
      if (thumbnail_id == PLAYLIST_THUMBNAIL_RIGHT)
         return playlist->right_thumbnail_mode;
      else if (thumbnail_id == PLAYLIST_THUMBNAIL_LEFT)
         return playlist->left_thumbnail_mode;
   }
   /* Fallback */
   return PLAYLIST_THUMBNAIL_MODE_DEFAULT;
}

bool playlist_thumbnail_match_with_filename(playlist_t *playlist)
{
   if (!playlist)
      return false;
   return playlist->thumbnail_match_mode == PLAYLIST_THUMBNAIL_MATCH_MODE_WITH_FILENAME;
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

const char *playlist_get_scan_database_name(playlist_t *playlist)
{
   if (!playlist)
      return NULL;
   return playlist->scan_record.database_name;
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

bool playlist_get_scan_omit_db_ref(playlist_t *playlist)
{
   if (!playlist)
      return false;
   return playlist->scan_record.omit_db_ref;
}

bool playlist_get_scan_overwrite_playlist(playlist_t *playlist)
{
   if (!playlist)
      return false;
   return playlist->scan_record.overwrite_playlist;
}

int playlist_get_scan_db_usage(playlist_t *playlist)
{
   if (!playlist)
      return 4;
   return playlist->scan_record.db_usage;
}

bool playlist_scan_refresh_enabled(playlist_t *playlist)
{
   if (!playlist)
      return false;
   return playlist->scan_record.content_dir && *playlist->scan_record.content_dir;
}

void playlist_set_default_core_path(playlist_t *playlist,
      const char *core_path)
{
   char real_core_path[PATH_MAX_LENGTH];
   if (!playlist || (!core_path || !*core_path))
      return;
   /* Get 'real' core path */
   strlcpy(real_core_path, core_path, sizeof(real_core_path));
   if (   !string_is_equal(real_core_path, FILE_PATH_DETECT)
       && !string_is_equal(real_core_path, FILE_PATH_BUILTIN))
       playlist_resolve_path(PLAYLIST_SAVE, true,
             real_core_path, sizeof(real_core_path));
   if (!*real_core_path)
      return;
   if (!string_is_equal(playlist->default_core_path, real_core_path))
   {
      if (playlist->default_core_path)
         free(playlist->default_core_path);
      playlist->default_core_path  = strdup(real_core_path);
      playlist->flags             |=  CNT_PLAYLIST_FLG_MOD;
   }
}

void playlist_set_default_core_name(
      playlist_t *playlist, const char *core_name)
{
   if (!playlist || (!core_name || !*core_name))
      return;

   if (!string_is_equal(playlist->default_core_name, core_name))
   {
      if (playlist->default_core_name)
         free(playlist->default_core_name);
      playlist->default_core_name  = strdup(core_name);
      playlist->flags             |=  CNT_PLAYLIST_FLG_MOD;
   }
}

void playlist_set_label_display_mode(playlist_t *playlist,
      enum playlist_label_display_mode label_display_mode)
{
   if (playlist && playlist->label_display_mode != label_display_mode)
   {
      playlist->label_display_mode = label_display_mode;
      playlist->flags             |=  CNT_PLAYLIST_FLG_MOD;
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
         playlist->flags               |=  CNT_PLAYLIST_FLG_MOD;
         break;
      case PLAYLIST_THUMBNAIL_LEFT:
         playlist->left_thumbnail_mode  = thumbnail_mode;
         playlist->flags               |=  CNT_PLAYLIST_FLG_MOD;
         break;
      case PLAYLIST_THUMBNAIL_ICON:
         /* should never be reached.  Do Nothing */
         break;

   }
}

void playlist_set_sort_mode(playlist_t *playlist,
      enum playlist_sort_mode sort_mode)
{
   if (playlist && playlist->sort_mode != sort_mode)
   {
      playlist->sort_mode = sort_mode;
      playlist->flags    |=  CNT_PLAYLIST_FLG_MOD;
   }
}

void playlist_set_scan_content_dir(playlist_t *playlist, const char *content_dir)
{
   bool current_string_empty;
   bool new_string_empty;
#if TARGET_OS_IPHONE
   char _tmpbuf[PATH_MAX_LENGTH];
   fill_pathname_abbreviate_special(_tmpbuf, content_dir, sizeof(_tmpbuf));
   content_dir = _tmpbuf;
#endif

   if (!playlist)
      return;

   current_string_empty = !playlist->scan_record.content_dir || !*playlist->scan_record.content_dir;
   new_string_empty     = !content_dir || !*content_dir;

   /* Check whether string value has changed
    * (note that a NULL or empty argument will
    * unset the playlist value) */
   if (    (current_string_empty && !new_string_empty)
       || (!current_string_empty &&  new_string_empty)
       || !string_is_equal(playlist->scan_record.content_dir, content_dir))
      playlist->flags    |=  CNT_PLAYLIST_FLG_MOD;
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

   current_string_empty = !playlist->scan_record.file_exts || !*playlist->scan_record.file_exts;
   new_string_empty     = !file_exts || !*file_exts;

   /* Check whether string value has changed
    * (note that a NULL or empty argument will
    * unset the playlist value) */
   if (   ( current_string_empty && !new_string_empty)
       || (!current_string_empty &&  new_string_empty)
       || !string_is_equal(playlist->scan_record.file_exts, file_exts))
      playlist->flags    |=  CNT_PLAYLIST_FLG_MOD;
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
#if TARGET_OS_IPHONE
   char _tmpbuf[PATH_MAX_LENGTH];
   fill_pathname_abbreviate_special(_tmpbuf, dat_file_path, sizeof(_tmpbuf));
   dat_file_path = _tmpbuf;
#endif

   if (!playlist)
      return;

   current_string_empty = !playlist->scan_record.dat_file_path || !*playlist->scan_record.dat_file_path;
   new_string_empty     = !dat_file_path || !*dat_file_path;

   /* Check whether string value has changed
    * (note that a NULL or empty argument will
    * unset the playlist value) */
   if (   ( current_string_empty && !new_string_empty)
       || (!current_string_empty &&  new_string_empty)
       || !string_is_equal(playlist->scan_record.dat_file_path, dat_file_path))
      playlist->flags    |=  CNT_PLAYLIST_FLG_MOD;
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

void playlist_set_scan_database_name(playlist_t *playlist, const char *database_name)
{
   bool current_string_empty;
   bool new_string_empty;

   if (!playlist)
      return;

   current_string_empty = !playlist->scan_record.database_name || !*playlist->scan_record.database_name;
   new_string_empty     = !database_name || !*database_name;

   /* Check whether string value has changed
    * (note that a NULL or empty argument will
    * unset the playlist value) */
   if (   ( current_string_empty && !new_string_empty)
       || (!current_string_empty &&  new_string_empty)
       || !string_is_equal(playlist->scan_record.database_name, database_name))
      playlist->flags    |=  CNT_PLAYLIST_FLG_MOD;
   else
      return; /* Strings are identical; do nothing */

   if (playlist->scan_record.database_name)
   {
      free(playlist->scan_record.database_name);
      playlist->scan_record.database_name = NULL;
   }

   if (!new_string_empty)
      playlist->scan_record.database_name = strdup(database_name);
}

void playlist_set_scan_search_recursively(playlist_t *playlist, bool search_recursively)
{
   if (playlist && playlist->scan_record.search_recursively != search_recursively)
   {
      playlist->scan_record.search_recursively = search_recursively;
      playlist->flags    |=  CNT_PLAYLIST_FLG_MOD;
   }
}

void playlist_set_scan_search_archives(playlist_t *playlist, bool search_archives)
{
   if (playlist && playlist->scan_record.search_archives != search_archives)
   {
      playlist->scan_record.search_archives = search_archives;
      playlist->flags    |=  CNT_PLAYLIST_FLG_MOD;
   }
}

void playlist_set_scan_filter_dat_content(playlist_t *playlist, bool filter_dat_content)
{
   if (playlist && playlist->scan_record.filter_dat_content != filter_dat_content)
   {
      playlist->scan_record.filter_dat_content = filter_dat_content;
      playlist->flags    |=  CNT_PLAYLIST_FLG_MOD;
   }
}

void playlist_set_scan_omit_db_ref(playlist_t *playlist, bool omit_db_ref)
{
   if (playlist && playlist->scan_record.omit_db_ref != omit_db_ref)
   {
      playlist->scan_record.omit_db_ref = omit_db_ref;
      playlist->flags    |=  CNT_PLAYLIST_FLG_MOD;
   }
}

void playlist_set_scan_db_usage(playlist_t *playlist, int db_usage)
{
   if (playlist && playlist->scan_record.db_usage != db_usage)
   {
      playlist->scan_record.db_usage = db_usage;
      playlist->flags    |=  CNT_PLAYLIST_FLG_MOD;
   }
}

void playlist_set_scan_overwrite_playlist(playlist_t *playlist, bool overwrite_playlist)
{
   if (playlist && playlist->scan_record.overwrite_playlist != overwrite_playlist)
   {
      playlist->scan_record.overwrite_playlist = overwrite_playlist;
      playlist->flags    |=  CNT_PLAYLIST_FLG_MOD;
   }
}

/* Returns true if specified entry has a valid
 * core association (i.e. a non-empty string
 * other than DETECT) */
bool playlist_entry_has_core(const struct playlist_entry *entry)
{
   if (  !entry
       || (!entry->core_path || !*entry->core_path)
       || (!entry->core_name || !*entry->core_name)
       || string_is_equal(entry->core_path, FILE_PATH_DETECT)
       || string_is_equal(entry->core_name, FILE_PATH_DETECT))
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
   if (playlist_entry_has_core(entry))
   {
      core_info_t *core_info = NULL;
      /* Search for associated core */
      if (core_info_find(entry->core_path, &core_info))
         return core_info;
   }
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

   if (  !playlist
       || (!playlist->default_core_path || !*playlist->default_core_path)
       || (!playlist->default_core_name || !*playlist->default_core_name)
       || string_is_equal(playlist->default_core_path, FILE_PATH_DETECT)
       || string_is_equal(playlist->default_core_name, FILE_PATH_DETECT))
      return NULL;

   /* Search for associated core */
   if (core_info_find(playlist->default_core_path, &core_info))
      return core_info;

   return NULL;
}
