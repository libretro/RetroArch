/* Copyright  (C) 2010-2019 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (manual_content_scan.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __MANUAL_CONTENT_SCAN_H
#define __MANUAL_CONTENT_SCAN_H

#include <retro_common_api.h>
#include <libretro.h>

#include <boolean.h>

#include <lists/string_list.h>
#include <lists/dir_list.h>
#include <formats/logiqx_dat.h>

#include "playlist.h"

RETRO_BEGIN_DECLS

enum manual_content_scan_method
{
   MANUAL_CONTENT_SCAN_METHOD_AUTOMATIC = 0,
   MANUAL_CONTENT_SCAN_METHOD_CUSTOM
};

/* Defines all possible system name types
 * > Use content directory name
 * > Use custom name
 * > Use database name */
enum manual_content_scan_system_name_type
{
   MANUAL_CONTENT_SCAN_SYSTEM_NAME_CONTENT_DIR = 0,
   MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
   MANUAL_CONTENT_SCAN_SYSTEM_NAME_AUTO,
   MANUAL_CONTENT_SCAN_SYSTEM_NAME_DATABASE
};

/* Defines types of matching content to databases */
enum manual_content_scan_db_usage
{
   MANUAL_CONTENT_SCAN_USE_DB_STRICT = 0,
   MANUAL_CONTENT_SCAN_USE_DB_LOOSE,
   MANUAL_CONTENT_SCAN_USE_DB_DAT_STRICT,
   MANUAL_CONTENT_SCAN_USE_DB_DAT_LOOSE,
   MANUAL_CONTENT_SCAN_USE_DB_NONE
};

/* Defines database selection types */
enum manual_content_scan_db_selection
{
   MANUAL_CONTENT_SCAN_SELECT_DB_AUTO = 0,
   MANUAL_CONTENT_SCAN_SELECT_DB_AUTO_FIRST_MATCH,
   MANUAL_CONTENT_SCAN_SELECT_DB_SPECIFIC
};

/* Defines all possible core name types
 * > Autodetect core (DETECT)
 * > Use manually set core */
enum manual_content_scan_core_type
{
   MANUAL_CONTENT_SCAN_CORE_DETECT = 0,
   MANUAL_CONTENT_SCAN_CORE_SET
};

/* Defines all possible return values for
 * manual_content_scan_validate_dat_file_path() */
enum manual_content_scan_dat_file_path_status
{
   MANUAL_CONTENT_SCAN_DAT_FILE_UNSET = 0,
   MANUAL_CONTENT_SCAN_DAT_FILE_OK,
   MANUAL_CONTENT_SCAN_DAT_FILE_INVALID,
   MANUAL_CONTENT_SCAN_DAT_FILE_TOO_LARGE
};

/* Defines all possible return values for
 * manual_content_scan_set_menu_from_playlist() */
enum manual_content_scan_playlist_refresh_status
{
   MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_OK = 0,
   MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_MISSING_CONFIG,
   MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_INVALID_CONTENT_DIR,
   MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_INVALID_SYSTEM_NAME,
   MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_INVALID_CORE,
   MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_INVALID_DAT_FILE,
   MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_DAT_FILE_TOO_LARGE
};

/* Defines all possible return values for
 * manual_content_scan_apply_overrides() */
enum manual_content_scan_override_status
{
   MANUAL_CONTENT_SCAN_OVR_OK = 0,
   MANUAL_CONTENT_SCAN_OVR_NO_SUCH_CORE,
   MANUAL_CONTENT_SCAN_OVR_AMBIGUOUS_CORE,
   MANUAL_CONTENT_SCAN_OVR_NO_SUCH_DATABASE,
   MANUAL_CONTENT_SCAN_OVR_NOT_A_DAT,
   MANUAL_CONTENT_SCAN_OVR_DAT_TOO_LARGE,
   MANUAL_CONTENT_SCAN_OVR_DAT_WITHOUT_MATCH
};

enum manual_content_scan_override_flags
{
   MANUAL_CONTENT_SCAN_OVR_MATCH        = (1 << 0), /* 'match' is set */
   MANUAL_CONTENT_SCAN_OVR_FIRST_DB     = (1 << 1),
   MANUAL_CONTENT_SCAN_OVR_NO_RECURSIVE = (1 << 2),
   MANUAL_CONTENT_SCAN_OVR_NO_ARCHIVES  = (1 << 3),
   MANUAL_CONTENT_SCAN_OVR_OVERWRITE    = (1 << 4),
   MANUAL_CONTENT_SCAN_OVR_VALIDATE     = (1 << 5),
   MANUAL_CONTENT_SCAN_OVR_OMIT_DB_REF  = (1 << 6)
};

/* Settings laid over a scan's own; a NULL string or a clear
 * flag leaves its setting as it is */
typedef struct manual_content_scan_overrides
{
   char *playlist;  /* --scan-playlist */
   char *database;  /* --scan-database=NAME */
   char *dat;       /* --scan-dat, as typed */
   char *core;      /* --scan-core, as typed */
   char *exts;      /* --scan-exts, space separated */
   enum manual_content_scan_db_usage match; /* STRICT, LOOSE or NONE */
   uint16_t flags;  /* MANUAL_CONTENT_SCAN_OVR_* */
} manual_content_scan_overrides_t;

/* Holds all configuration parameters required
 * for a manual content scan task */
typedef struct
{
   char core_name[NAME_MAX_LENGTH];
   char system_name[NAME_MAX_LENGTH];
   char database_name[NAME_MAX_LENGTH];
   char content_dir[PATH_MAX_LENGTH];
   char playlist_file[PATH_MAX_LENGTH];
   char core_path[PATH_MAX_LENGTH];
   char file_exts[PATH_MAX_LENGTH];
   char dat_file_path[PATH_MAX_LENGTH];
   bool core_set;
   bool file_exts_custom_set;
   bool search_recursively;
   bool search_archives;
   bool filter_dat_content;
   bool overwrite_playlist;
   bool validate_entries;
   bool omit_db_reference;
   bool target_is_single_determined_playlist;
   bool content_is_file;
   enum manual_content_scan_db_usage db_usage;
   enum manual_content_scan_db_selection db_selection;
   /* Captured when the config is built, on the main thread: the scan
    * handler runs on the threaded task queue's worker and reads these
    * from its config, never from the live settings or runloop. */
   bool cli_scan_output;
   bool scan_serial_and_crc;
} manual_content_scan_task_config_t;

/*****************/
/* Configuration */
/*****************/

/* Pointer access
 * > This is a little ugly, but it allows us to
 *   make use of standard 'menu_settings' code
 *   for several config parameters (rather than
 *   implementing unnecessary custom menu entries) */

/* Returns a pointer to the internal
 * 'content_dir' string */
char *manual_content_scan_get_content_dir_ptr(void);

unsigned manual_content_scan_get_scan_method_enum(void);
unsigned manual_content_scan_get_scan_use_db_enum(void);
unsigned manual_content_scan_get_scan_db_select_enum(void);

char   *manual_content_scan_get_scan_db_select_custom_ptr(void);
size_t *manual_content_scan_get_scan_db_select_custom_size(void);

/* Returns a pointer to the internal
 * 'system_name_custom' string */
char *manual_content_scan_get_system_name_custom_ptr(void);

/* Returns size of the internal
 * 'system_name_custom' string */
size_t manual_content_scan_get_system_name_custom_size(void);

/* Returns a pointer to the internal
 * 'file_exts_custom' string */
char *manual_content_scan_get_file_exts_custom_ptr(void);

/* Returns size of the internal
 * 'file_exts_custom' string */
size_t manual_content_scan_get_file_exts_custom_size(void);

/* Returns custom extensions,
 * falling back to core-supported extensions */
const char *manual_content_scan_get_file_exts(void);

/* Returns a pointer to the internal
 * 'dat_file_path' string */
char *manual_content_scan_get_dat_file_path_ptr(void);

/* Returns size of the internal
 * 'dat_file_path' string */
size_t manual_content_scan_get_dat_file_path_size(void);

/* Returns a pointer to the internal
 * 'search_recursively' bool */
bool *manual_content_scan_get_search_recursively_ptr(void);

bool *manual_content_scan_get_omit_db_ref_ptr(void);
/* Returns a pointer to the internal
 * 'search_archives' bool */
bool *manual_content_scan_get_search_archives_ptr(void);

/* Returns a pointer to the internal
 * 'filter_dat_content' bool */
bool *manual_content_scan_get_filter_dat_content_ptr(void);

/* Returns a pointer to the internal
 * 'overwrite_playlist' bool */
bool *manual_content_scan_get_overwrite_playlist_ptr(void);

/* Returns a pointer to the internal
 * 'validate_entries' bool */
bool *manual_content_scan_get_validate_entries_ptr(void);

/* Sanitisation */

/* Removes invalid characters from
 * 'system_name_custom' string */
void manual_content_scan_scrub_system_name_custom(void);

/* Removes period (full stop) characters from
 * 'file_exts_custom' string and converts to
 * lower case */
void manual_content_scan_scrub_file_exts_custom(void);

/* Checks 'dat_file_path' string and resets it
 * if invalid */
enum manual_content_scan_dat_file_path_status
      manual_content_scan_validate_dat_file_path(void);

/* Menu setters */

/* Sets every setting back to its default */
void manual_content_scan_reset_settings(void);

/* The Manual Scan settings as they are, for a scan chain to put back
 * once it has run its own: NULL out of memory. Restoring frees it. */
typedef struct manual_content_scan_saved manual_content_scan_saved_t;
manual_content_scan_saved_t *manual_content_scan_save_settings(void);
void manual_content_scan_restore_settings(manual_content_scan_saved_t *saved);

/* Sets content directory for next manual scan
 * operation.
 * Returns true if content directory is valid. */
bool manual_content_scan_set_menu_content_dir(const char *content_dir);

/* What the file system says of one scan's paths, read off the main
 * thread by manual_content_scan_fs_read(), so that the _fs setters
 * below ask it nothing.  Only the paths it was read for: it holds no
 * reference to the settings. */
typedef struct manual_content_scan_fs
{
   char *ov_dat;         /* @ov_dat expanded and resolved, or NULL */
   char *ov_core;        /* @ov_core resolved when it is a path, or NULL */
   int64_t dat_size;     /* the saved DAT's size; <= 0: not there */
   int64_t ov_dat_size;  /* likewise, @ov_dat's */
   int content_stat;     /* path_stat() of the content folder */
} manual_content_scan_fs_t;

/* Any thread: reads nothing but its arguments and the file system.
 * @content_dir and @dat as the settings hold them, before expansion;
 * @ov_core and @ov_dat an item's overrides, each NULL when unset.
 * NULL out of memory.  Free with manual_content_scan_fs_free(). */
manual_content_scan_fs_t *manual_content_scan_fs_read(
      const char *content_dir, const char *dat,
      const char *ov_core, const char *ov_dat);
void manual_content_scan_fs_free(manual_content_scan_fs_t *fs);

/* As manual_content_scan_set_menu_content_dir(), the folder's stat
 * taken from @fs (NULL: asked now) */
bool manual_content_scan_set_menu_content_dir_fs(const char *content_dir,
      const manual_content_scan_fs_t *fs);

bool manual_content_scan_set_menu_scan_method(
      enum manual_content_scan_method method);

bool manual_content_scan_set_menu_scan_use_db(
      enum manual_content_scan_db_usage usage);

bool manual_content_scan_set_menu_scan_db_select(
      enum manual_content_scan_db_selection select,
      const char *db_name);

/* Sets system name for the next manual scan
 * operation.
 * Returns true if system name is valid.
 * NOTE:
 * > Only sets 'system_name_type' and 'system_name_database'
 * > 'system_name_content_dir' and 'system_name_custom' are
 *   (by necessity) handled elsewhere
 * > This may look fishy, but it's not - it's a menu-specific
 *   function, and this is simply the cleanest way to handle
 *   the setting... */
bool manual_content_scan_set_menu_system_name(
      enum manual_content_scan_system_name_type system_name_type,
      const char *system_name);

/* Sets core name for the next manual scan
 * operation (+ core path and other associated
 * parameters).
 * Returns true if core name is valid. */
bool manual_content_scan_set_menu_core_name(
      enum manual_content_scan_core_type core_type,
      const char *core_name);

/* Sets all parameters for the next manual scan
 * operation according the to recorded values in
 * the specified playlist.
 * Returns MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_OK
 * if playlist contains a valid scan record. */
enum manual_content_scan_playlist_refresh_status
      manual_content_scan_set_menu_from_playlist(playlist_t *playlist,
            const char *path_content_database, bool show_hidden_files);

/* As manual_content_scan_set_menu_from_playlist(), with the
 * paths of the installed databases in @rdb_list (NULL: none). */
enum manual_content_scan_playlist_refresh_status
      manual_content_scan_set_menu_from_playlist_list(playlist_t *playlist,
            const struct string_list *rdb_list);

/* As manual_content_scan_set_menu_from_playlist_list(), the folder
 * and the DAT as @fs read them (NULL: asked now) */
enum manual_content_scan_playlist_refresh_status
      manual_content_scan_set_menu_from_playlist_list_fs(
            playlist_t *playlist, const struct string_list *rdb_list,
            const manual_content_scan_fs_t *fs);

manual_content_scan_overrides_t *manual_content_scan_overrides_new(void);
void manual_content_scan_overrides_free(manual_content_scan_overrides_t *ov);

/* Lays @ov over the current settings and sets the Custom
 * method, stopping at the first override that does not
 * resolve; NULL @ov changes nothing.
 * > @rdb_list: paths of the installed databases, or NULL
 * > @count: the cores @ov->core matched, when not NULL */
enum manual_content_scan_override_status
      manual_content_scan_apply_overrides(
            const manual_content_scan_overrides_t *ov,
            const struct string_list *rdb_list, unsigned *count);

/* As manual_content_scan_apply_overrides(), the core path and the
 * DAT as @fs read them for @ov (NULL: asked now) */
enum manual_content_scan_override_status
      manual_content_scan_apply_overrides_fs(
            const manual_content_scan_overrides_t *ov,
            const struct string_list *rdb_list, unsigned *count,
            const manual_content_scan_fs_t *fs);

/* Menu getters */

/* Fetches content directory for next manual scan
 * operation.
 * Returns true if content directory is valid. */
bool manual_content_scan_get_menu_content_dir(const char **content_dir);

bool manual_content_scan_get_menu_scan_method(const char **scan_method);
bool manual_content_scan_get_menu_scan_use_db(const char **scan_use_db);
bool manual_content_scan_get_menu_scan_db_select(const char **scan_db_select);

/* Fetches system name for the next manual scan operation.
 * Returns true if system name is valid.
 * NOTE: This corresponds to the 'System Name' value
 * displayed in menus - this is not identical to the
 * actual system name used when generating the playlist */
bool manual_content_scan_get_menu_system_name(const char **system_name);
unsigned manual_content_scan_get_menu_system_name_type(void);

/* Fetches core name for the next manual scan operation.
 * Returns true if core name is valid. */
bool manual_content_scan_get_menu_core_name(const char **core_name);

/* Menu utility functions */

struct string_list *manual_content_scan_get_menu_scan_method_list(void);
struct string_list *manual_content_scan_get_menu_scan_use_db_list(void);
struct string_list *manual_content_scan_get_menu_scan_db_select_list(
               const char *path_content_database, bool show_hidden_files);

/* Creates a list of all possible 'system name' menu
 * strings, for use in 'menu_displaylist' drop-down
 * lists and 'menu_cbs_left/right'
 * > Returns NULL in the event of failure
 * > Returned string list must be free()'d */
struct string_list *manual_content_scan_get_menu_system_name_list(
      const char *path_content_database, bool show_hidden_files);

/* Creates a list of all possible 'core name' menu
 * strings, for use in 'menu_displaylist' drop-down
 * lists and 'menu_cbs_left/right'
 * > Returns NULL in the event of failure
 * > Returned string list must be free()'d */
struct string_list *manual_content_scan_get_menu_core_name_list(void);

/****************/
/* Task Helpers */
/****************/

/* True if a scan of the current settings writes one playlist
 * (target_is_single_determined_playlist). */
bool manual_content_scan_is_single_playlist(void);

/* Parses current manual content scan settings,
 * and extracts all information required to configure
 * a manual content scan task.
 * Returns false if current settings are invalid. */
bool manual_content_scan_get_task_config(
      manual_content_scan_task_config_t *task_config,
      const char *path_dir_playlist
      );

/* Performs rudimentary validation of a Logiqx XML DAT
 * file path: extension whitelist (.dat / .xml,
 * case-insensitive), existence, size > 0, with the
 * size optionally returned for free-memory checks.
 * The parser itself (logiqx_dat.c) is path-agnostic
 * and performs no validation or I/O; every consumer
 * of a DAT path goes through this one predicate. */
bool manual_content_scan_dat_path_is_valid(const char *path,
      uint64_t *file_size);

/* Creates a list of all valid content in the specified
 * content directory
 * > Returns NULL in the event of failure
 * > Returned string list must be free()'d */
struct string_list *manual_content_scan_get_content_list(
      manual_content_scan_task_config_t *task_config);

/* Resumable counterpart of
 * manual_content_scan_get_content_list(): creates
 * @*list and, when the content dir is a directory, a
 * dir_list_iter_t in @*iter that fills it under the
 * caller's budget via dir_list_iter_step().  The same
 * extension / compressed-file policy is applied.  A
 * content dir that is a plain file fills the list here
 * and leaves *iter NULL.  When the walk completes the
 * caller owes the list the same treatment the blocking
 * getter applied: reject it when empty, then
 * dir_list_sort(list, true).
 * > Returns false (leaving *list / *iter NULL) if
 *   config is invalid or the directory cannot be
 *   opened */
bool manual_content_scan_content_list_iter_new(
      manual_content_scan_task_config_t *task_config,
      struct string_list **list, dir_list_iter_t **iter);

/* Adds specified content to playlist, if not already
 * present */
void manual_content_scan_add_content_to_playlist(
      manual_content_scan_task_config_t *task_config,
      playlist_t *playlist, const char *content_path,
      int content_type, logiqx_dat_t *dat_file);

bool manual_content_scan_get_playlist_content_label(
      const char *content_path, logiqx_dat_t *dat_file,
      bool filter_dat_content,
      char *s, size_t len);
bool manual_content_scan_get_playlist_content_path(
      manual_content_scan_task_config_t *task_config,
      const char *content_path, int content_type,
      char *s, size_t len);

RETRO_END_DECLS

#endif
