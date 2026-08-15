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

#ifndef _PLAYLIST_H__
#define _PLAYLIST_H__

#include <boolean.h>
#include <stddef.h>

#include <retro_common_api.h>
#include <retro_miscellaneous.h>
#include <lists/string_list.h>

#include "core_info.h"

/* Default maximum playlist size */
#define COLLECTION_SIZE 0x7FFFFFFF

RETRO_BEGIN_DECLS

enum playlist_runtime_status
{
   PLAYLIST_RUNTIME_UNKNOWN = 0,
   PLAYLIST_RUNTIME_MISSING,
   PLAYLIST_RUNTIME_VALID
};

enum playlist_file_mode
{
   PLAYLIST_LOAD = 0,
   PLAYLIST_SAVE
};

enum playlist_label_display_mode
{
   LABEL_DISPLAY_MODE_DEFAULT = 0,
   LABEL_DISPLAY_MODE_REMOVE_PARENTHESES,
   LABEL_DISPLAY_MODE_REMOVE_BRACKETS,
   LABEL_DISPLAY_MODE_REMOVE_PARENTHESES_AND_BRACKETS,
   LABEL_DISPLAY_MODE_KEEP_REGION,
   LABEL_DISPLAY_MODE_KEEP_DISC_INDEX,
   LABEL_DISPLAY_MODE_KEEP_REGION_AND_DISC_INDEX
};

enum playlist_thumbnail_mode
{
   PLAYLIST_THUMBNAIL_MODE_DEFAULT = 0,
   PLAYLIST_THUMBNAIL_MODE_OFF,
   PLAYLIST_THUMBNAIL_MODE_SCREENSHOTS,
   PLAYLIST_THUMBNAIL_MODE_TITLE_SCREENS,
   PLAYLIST_THUMBNAIL_MODE_BOXARTS,
   PLAYLIST_THUMBNAIL_MODE_LOGOS,

   PLAYLIST_THUMBNAIL_MODE_LAST
};

enum playlist_thumbnail_match_mode
{
   PLAYLIST_THUMBNAIL_MATCH_MODE_DEFAULT = 0,
   PLAYLIST_THUMBNAIL_MATCH_MODE_WITH_LABEL = PLAYLIST_THUMBNAIL_MATCH_MODE_DEFAULT,
   PLAYLIST_THUMBNAIL_MATCH_MODE_WITH_FILENAME
};

enum playlist_sort_mode
{
   PLAYLIST_SORT_MODE_DEFAULT = 0,
   PLAYLIST_SORT_MODE_ALPHABETICAL,
   PLAYLIST_SORT_MODE_OFF
};

/* Note: We already have a left/right enum defined
 * in gfx_thumbnail_path.h - but we can't include
 * menu code here, so have to make a 'duplicate'... */
enum playlist_thumbnail_id
{
   PLAYLIST_THUMBNAIL_RIGHT = 0,
   PLAYLIST_THUMBNAIL_LEFT,
   PLAYLIST_THUMBNAIL_ICON
};

enum playlist_thumbnail_name_flags
{
   PLAYLIST_THUMBNAIL_FLAG_INVALID          = 0,
   PLAYLIST_THUMBNAIL_FLAG_FULL_NAME        = (1 << 0),
   PLAYLIST_THUMBNAIL_FLAG_STD_NAME         = (1 << 1),
   PLAYLIST_THUMBNAIL_FLAG_SHORT_NAME       = (1 << 2),
   PLAYLIST_THUMBNAIL_FLAG_NONE             = (1 << 3)
};

typedef struct content_playlist playlist_t;

/* Holds all parameters required to uniquely
 * identify a playlist content path */
typedef struct
{
   char *real_path;
   char *archive_path;
   uint32_t real_path_hash;
   uint32_t archive_path_hash;
   bool is_archive;
   bool is_in_archive;
} playlist_path_id_t;

struct playlist_entry
{
   char *path;
   char *label;
   char *core_path;
   char *core_name;
   char *db_name;
   char *crc32;
   char *subsystem_ident;
   char *subsystem_name;
   char *runtime_str;
   char *last_played_str;
   struct string_list *subsystem_roms;
   playlist_path_id_t *path_id;
   unsigned entry_slot;
   unsigned runtime_hours;
   unsigned runtime_minutes;
   unsigned runtime_seconds;
   /* Note: due to platform dependence, have to record
    * timestamp as either a string or independent integer
    * values. The latter is more verbose, but more efficient. */
   unsigned last_played_year;
   unsigned last_played_month;
   unsigned last_played_day;
   unsigned last_played_hour;
   unsigned last_played_minute;
   unsigned last_played_second;
   enum playlist_runtime_status runtime_status;
   int thumbnail_flags;
};

/* Holds all configuration parameters required
 * when initialising/saving playlists */
typedef struct
{
   size_t capacity;
   bool old_format;
   bool compress;
   bool fuzzy_archive_match;
   bool autofix_paths;
   char path[PATH_MAX_LENGTH];
   char base_content_directory[DIR_MAX_LENGTH];
} playlist_config_t;

/* Convenience function: copies specified playlist
 * path to specified playlist configuration object */
size_t playlist_config_set_path(playlist_config_t *config, const char *path);

/* Convenience function: copies base content directory
 * path to specified playlist configuration object */
size_t playlist_config_set_base_content_directory(playlist_config_t* config, const char* path);

/* Creates a copy of the specified playlist configuration.
 * Returns false in the event of an error */
bool playlist_config_copy(const playlist_config_t *src, playlist_config_t *dst);

/* Returns internal playlist configuration object
 * of specified playlist.
 * Returns NULL it the event of an error. */
playlist_config_t *playlist_get_config(playlist_t *playlist);

/**
 * playlist_init:
 * @config            	: Playlist configuration object.
 *
 * Creates and initializes a playlist.
 *
 * Returns: handle to new playlist if successful, otherwise NULL
 **/
playlist_t *playlist_init(const playlist_config_t *config);

/* Resumable playlist parse.
 *
 * playlist_parse_begin() opens and sniffs the playlist file and
 * returns a parse handle (NULL only on allocation failure; a missing
 * or unreadable file resolves - as it always has - to an empty
 * playlist at the first step).  playlist_parse_step() advances the
 * parse, consulting @budget_cb between batches of work when
 * non-NULL: it returns 1 when the playlist is complete, 0 when the
 * budget ran out mid-parse (call again), and -1 on a failed parse.
 * With a NULL @budget_cb the step runs to completion -
 * playlist_init() is exactly begin + one unbudgeted step + end, so
 * the blocking and budgeted paths cannot drift apart.
 * playlist_parse_end() returns the finished playlist (or NULL after
 * a failure) and frees the handle; playlist_parse_abort() abandons a
 * parse at any point, releasing everything. */
typedef struct playlist_parse playlist_parse_t;

playlist_parse_t *playlist_parse_begin(const playlist_config_t *config);
int playlist_parse_step(playlist_parse_t *p,
      bool (*budget_cb)(void *), void *budget_ud);
playlist_t *playlist_parse_end(playlist_parse_t *p);
void playlist_parse_abort(playlist_parse_t *p);

/* Deferred variant of playlist_init_cached(): same contract, spread
 * over budgeted steps.  playlist_init_cached_deferred() returns 1
 * when the requested playlist is cached and ready (cache hit, or the
 * parse completed within budget), 0 while a parse is pending, -1 on
 * failure.  While pending, playlist_init_cached_continue() advances
 * the parse (it never touches the global cache, so a worker task may
 * drive it; 1 means the parse finished and
 * playlist_init_cached_finish() - main thread - must install it).
 * playlist_init_cached_defer_abort() abandons any pending parse. */
int playlist_init_cached_deferred(const playlist_config_t *config,
      bool (*budget_cb)(void *), void *budget_ud);
int playlist_init_cached_continue(bool (*budget_cb)(void *), void *budget_ud);
int playlist_init_cached_finish(void);
void playlist_init_cached_defer_abort(void);

/**
 * playlist_free:
 * @playlist        	   : Playlist handle.
 *
 * Frees playlist handle.
 */
void playlist_free(playlist_t *playlist);

/**
 * playlist_clear:
 * @playlist        	   : Playlist handle.
 *
 * Clears all playlist entries in playlist.
 **/
void playlist_clear(playlist_t *playlist);

/**
 * playlist_size:
 * @playlist        	   : Playlist handle.
 *
 * Gets size of playlist.
 * Returns: size of playlist.
 **/
size_t playlist_size(playlist_t *playlist);

/**
 * playlist_capacity:
 * @playlist        	   : Playlist handle.
 *
 * Gets maximum capacity of playlist.
 * Returns: maximum capacity of playlist.
 **/
size_t playlist_capacity(playlist_t *playlist);

/**
 * playlist_get_index:
 * @playlist               : Playlist handle.
 * @idx                 : Index of playlist entry.
 *
 * Gets values of playlist index:
 **/
void playlist_get_index(playlist_t *playlist,
      size_t idx,
      const struct playlist_entry **entry);

/**
 * playlist_delete_index:
 * @playlist               : Playlist handle.
 * @idx                 : Index of playlist entry.
 *
 * Deletes the entry at index:
 **/
void playlist_delete_index(playlist_t *playlist,
      size_t idx);

/**
 * playlist_delete_by_path:
 * @playlist            : Playlist handle.
 * @search_path         : Content path.
 *
 * Deletes all entries with content path
 * matching 'search_path'
 **/
void playlist_delete_by_path(playlist_t *playlist,
      const char *search_path);

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
      bool is_core, char *path, size_t len);

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
bool playlist_content_path_is_valid(const char *path);

/**
 * playlist_push:
 * @playlist        	   : Playlist handle.
 *
 * Push entry to top of playlist.
 **/
bool playlist_push(playlist_t *playlist,
      const struct playlist_entry *entry);

/**
 * playlist_push_unchecked:
 *
 * Appends @entry at the front of @playlist WITHOUT searching for an
 * existing matching entry.  The caller must have already proven the
 * entry's content path absent (via playlist_entry_exists() or a
 * playlist_dedup_t index); pushing a path that is present creates a
 * duplicate.  Applies the same validation, capacity and eviction
 * rules as playlist_push().
 **/
bool playlist_push_unchecked(playlist_t *playlist,
      const struct playlist_entry *entry);

bool playlist_push_runtime(playlist_t *playlist,
      const struct playlist_entry *entry);

void playlist_update(playlist_t *playlist, size_t idx,
      const struct playlist_entry *update_entry);

/* Note: register_update determines whether the internal
 * 'playlist->modified' flag is set when updating runtime
 * values. Since these are normally set temporarily (for
 * display purposes), we do not always want this function
 * to trigger a re-write of the playlist file. */
void playlist_update_runtime(playlist_t *playlist, size_t idx,
      const struct playlist_entry *update_entry,
      bool register_update);

void playlist_update_thumbnail_name_flag(playlist_t *playlist, size_t idx,
     enum playlist_thumbnail_name_flags thumbnail_flags);
enum playlist_thumbnail_name_flags playlist_get_next_thumbnail_name_flag(playlist_t *playlist, size_t idx);
enum playlist_thumbnail_name_flags playlist_get_curr_thumbnail_name_flag(playlist_t *playlist, size_t idx);

void playlist_get_index_by_path(playlist_t *playlist,
      const char *search_path,
      const struct playlist_entry **entry);

bool playlist_entry_exists(playlist_t *playlist,
      const char *path);

/* Content path dedup index: answers playlist_entry_exists() queries
 * in O(1) expected time for repeated probes against the same
 * playlist.  Verified candidates go through the same matching rules
 * as the linear scan (including fuzzy archive matching), so answers
 * are identical; internal allocation failure degrades the index to
 * the linear scan transparently.  The index owns all of its state
 * and may be freed before or after the playlist. */
typedef struct playlist_dedup playlist_dedup_t;

playlist_dedup_t *playlist_dedup_init(void);

/**
 * playlist_dedup_seed_step:
 *
 * Indexes @playlist's current entries, resuming from where the
 * previous call stopped.  When @budget_cb is non-NULL it is
 * consulted between entries and seeding yields (returning false)
 * once it reports the budget exhausted; at least one entry is
 * seeded per call.  Returns true when seeding has completed.
 **/
bool playlist_dedup_seed_step(playlist_dedup_t *dedup,
      playlist_t *playlist,
      bool (*budget_cb)(void *userdata), void *userdata);

/**
 * playlist_dedup_check_add:
 *
 * Returns what playlist_entry_exists(@playlist, @path) would
 * return.  When @will_add is true and the path was absent, the
 * path is recorded in the index so that subsequent queries see it;
 * the caller is expected to push the corresponding entry.
 **/
bool playlist_dedup_check_add(playlist_dedup_t *dedup,
      playlist_t *playlist, const char *path, bool will_add);

void playlist_dedup_free(playlist_dedup_t *dedup);

char *playlist_get_conf_path(playlist_t *playlist);

uint32_t playlist_get_size(playlist_t *playlist);

void playlist_write_file(playlist_t *playlist);

void playlist_write_runtime_file(playlist_t *playlist);

void playlist_qsort(playlist_t *playlist);

void playlist_free_cached(void);

playlist_t *playlist_get_cached(void);

/* If current on-disk playlist file referenced
 * by 'config->path' does not match requested
 * 'old format' or 'compression' state, file will
 * be updated automatically
 * > Since this function is called whenever a
 *   playlist is browsed via the menu, this is
 *   a simple method for ensuring that files
 *   are always kept synced with user settings */
bool playlist_init_cached(const playlist_config_t *config);

void command_playlist_push_write(
      playlist_t *playlist,
      const struct playlist_entry *entry);

void command_playlist_update_write(
      playlist_t *playlist,
      size_t idx,
      const struct playlist_entry *entry);

/* Returns true if specified playlist index matches
 * specified content/core paths */
bool playlist_index_is_valid(playlist_t *playlist, size_t idx,
      const char *path, const char *core_path);

/* Returns true if specified playlist entries have
 * identical content and core paths */
bool playlist_entries_are_equal(
      const struct playlist_entry *entry_a,
      const struct playlist_entry *entry_b,
      const playlist_config_t *config);

/* Returns true if entries at specified indices
 * of specified playlist have identical content
 * and core paths */
bool playlist_index_entries_are_equal(
      playlist_t *playlist, size_t idx_a, size_t idx_b);

void playlist_get_crc32(playlist_t *playlist, size_t idx,
      const char **crc32);

/* If db_name is empty, 'returns' playlist file basename */
void playlist_get_db_name(playlist_t *playlist, size_t idx,
      const char **db_name);

const char *playlist_get_default_core_path(playlist_t *playlist);
const char *playlist_get_default_core_name(playlist_t *playlist);
enum playlist_label_display_mode playlist_get_label_display_mode(playlist_t *playlist);
enum playlist_thumbnail_mode playlist_get_thumbnail_mode(
      playlist_t *playlist, enum playlist_thumbnail_id thumbnail_id);
bool playlist_thumbnail_match_with_filename(playlist_t *playlist);
enum playlist_sort_mode playlist_get_sort_mode(playlist_t *playlist);
const char *playlist_get_scan_content_dir(playlist_t *playlist);
const char *playlist_get_scan_file_exts(playlist_t *playlist);
const char *playlist_get_scan_dat_file_path(playlist_t *playlist);
const char *playlist_get_scan_database_name(playlist_t *playlist);
bool playlist_get_scan_search_recursively(playlist_t *playlist);
bool playlist_get_scan_search_archives(playlist_t *playlist);
bool playlist_get_scan_filter_dat_content(playlist_t *playlist);
bool playlist_get_scan_omit_db_ref(playlist_t *playlist);
bool playlist_get_scan_overwrite_playlist(playlist_t *playlist);
int playlist_get_scan_db_usage(playlist_t *playlist);
bool playlist_scan_refresh_enabled(playlist_t *playlist);

void playlist_set_default_core_path(playlist_t *playlist, const char *core_path);
void playlist_set_default_core_name(playlist_t *playlist, const char *core_name);
void playlist_set_label_display_mode(playlist_t *playlist, enum playlist_label_display_mode label_display_mode);
void playlist_set_thumbnail_mode(
      playlist_t *playlist, enum playlist_thumbnail_id thumbnail_id, enum playlist_thumbnail_mode thumbnail_mode);
void playlist_set_sort_mode(playlist_t *playlist, enum playlist_sort_mode sort_mode);
void playlist_set_scan_content_dir(playlist_t *playlist, const char *content_dir);
void playlist_set_scan_file_exts(playlist_t *playlist, const char *file_exts);
void playlist_set_scan_dat_file_path(playlist_t *playlist, const char *dat_file_path);
void playlist_set_scan_database_name(playlist_t *playlist, const char *database_name);
void playlist_set_scan_search_recursively(playlist_t *playlist, bool search_recursively);
void playlist_set_scan_search_archives(playlist_t *playlist, bool search_archives);
void playlist_set_scan_filter_dat_content(playlist_t *playlist, bool filter_dat_content);
void playlist_set_scan_omit_db_ref(playlist_t *playlist, bool omit_db_ref);
void playlist_set_scan_overwrite_playlist(playlist_t *playlist, bool overwrite_playlist);
void playlist_set_scan_db_usage(playlist_t *playlist, int db_usage);

/* Returns true if specified entry has a valid
 * core association (i.e. a non-empty string
 * other than DETECT) */
bool playlist_entry_has_core(const struct playlist_entry *entry);

/* Fetches core info object corresponding to the
 * currently associated core of the specified
 * playlist entry.
 * Returns NULL if entry does not have a valid
 * core association */
core_info_t *playlist_entry_get_core_info(const struct playlist_entry* entry);

/* Fetches core info object corresponding to the
 * currently associated default core of the
 * specified playlist.
 * Returns NULL if playlist does not have a valid
 * default core association */
core_info_t *playlist_get_default_core_info(playlist_t* playlist);

void playlist_set_cached_external(playlist_t* pl);

RETRO_END_DECLS

#endif
