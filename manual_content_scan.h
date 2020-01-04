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
#include <formats/logiqx_dat.h>

#include "playlist.h"

RETRO_BEGIN_DECLS

/* Defines all possible system name types
 * > Use content directory name
 * > Use custom name
 * > Use database name */
enum manual_content_scan_system_name_type
{
   MANUAL_CONTENT_SCAN_SYSTEM_NAME_CONTENT_DIR = 0,
   MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
   MANUAL_CONTENT_SCAN_SYSTEM_NAME_DATABASE
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

/* Holds all configuration parameters required
 * for a manual content scan task */
typedef struct
{
   char playlist_file[PATH_MAX_LENGTH];
   char content_dir[PATH_MAX_LENGTH];
   char system_name[PATH_MAX_LENGTH];
   char database_name[PATH_MAX_LENGTH];
   char core_name[PATH_MAX_LENGTH];
   char core_path[PATH_MAX_LENGTH];
   char file_exts[PATH_MAX_LENGTH];
   char dat_file_path[PATH_MAX_LENGTH];
   bool core_set;
   bool search_archives;
   bool overwrite_playlist;
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

/* Returns a pointer to the internal
 * 'dat_file_path' string */
char *manual_content_scan_get_dat_file_path_ptr(void);

/* Returns size of the internal
 * 'dat_file_path' string */
size_t manual_content_scan_get_dat_file_path_size(void);

/* Returns a pointer to the internal
 * 'search_archives' bool */
bool *manual_content_scan_get_search_archives_ptr(void);

/* Returns a pointer to the internal
 * 'overwrite_playlist' bool */
bool *manual_content_scan_get_overwrite_playlist_ptr(void);

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

/* Sets content directory for next manual scan
 * operation.
 * Returns true if content directory is valid. */
bool manual_content_scan_set_menu_content_dir(const char *content_dir);

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

/* Menu getters */

/* Fetches content directory for next manual scan
 * operation.
 * Returns true if content directory is valid. */
bool manual_content_scan_get_menu_content_dir(const char **content_dir);

/* Fetches system name for the next manual scan operation.
 * Returns true if system name is valid.
 * NOTE: This corresponds to the 'System Name' value
 * displayed in menus - this is not identical to the
 * actual system name used when generating the playlist */
bool manual_content_scan_get_menu_system_name(const char **system_name);

/* Fetches core name for the next manual scan operation.
 * Returns true if core name is valid. */
bool manual_content_scan_get_menu_core_name(const char **core_name);

/* Menu utility functions */

/* Creates a list of all possible 'system name' menu
 * strings, for use in 'menu_displaylist' drop-down
 * lists and 'menu_cbs_left/right'
 * > Returns NULL in the event of failure
 * > Returned string list must be free()'d */
struct string_list *manual_content_scan_get_menu_system_name_list(
      const char *path_content_database);

/* Creates a list of all possible 'core name' menu
 * strings, for use in 'menu_displaylist' drop-down
 * lists and 'menu_cbs_left/right'
 * > Returns NULL in the event of failure
 * > Returned string list must be free()'d */
struct string_list *manual_content_scan_get_menu_core_name_list(void);

/****************/
/* Task Helpers */
/****************/

/* Parses current manual content scan settings,
 * and extracts all information required to configure
 * a manual content scan task.
 * Returns false if current settings are invalid. */
bool manual_content_scan_get_task_config(
      manual_content_scan_task_config_t *task_config,
      const char *path_dir_playlist
      );

/* Creates a list of all valid content in the specified
 * content directory
 * > Returns NULL in the event of failure
 * > Returned string list must be free()'d */
struct string_list *manual_content_scan_get_content_list(manual_content_scan_task_config_t *task_config);

/* Adds specified content to playlist, if not already
 * present */
void manual_content_scan_add_content_to_playlist(
      manual_content_scan_task_config_t *task_config,
      playlist_t *playlist, const char *content_path,
      int content_type, logiqx_dat_t *dat_file,
      bool fuzzy_archive_match);

RETRO_END_DECLS

#endif
