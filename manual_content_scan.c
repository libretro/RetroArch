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

#include <memory/mem_stats.h>
#include <file/file_path.h>
#include <file/archive_file.h>
#include <string/stdstring.h>
#include <lists/dir_list.h>
#include <retro_miscellaneous.h>

#include <compat/strl.h>

#include "msg_hash.h"
#include "list_special.h"
#include "core_info.h"
#include "file_path_special.h"

#include "frontend/frontend_driver.h"

#include "manual_content_scan.h"
#include "retroarch.h"

/* Holds all configuration parameters associated
 * with a manual content scan */
typedef struct
{
   enum manual_content_scan_method scan_method;
   enum manual_content_scan_system_name_type system_name_type;
   enum manual_content_scan_core_type core_type;
   enum manual_content_scan_db_usage db_usage;
   enum manual_content_scan_db_selection db_selection;


   bool search_recursively;
   bool search_archives;
   bool omit_db_ref;
   bool filter_dat_content;
   bool overwrite_playlist;
   bool validate_entries;
} scan_settings_t;

#define MANUAL_CONTENT_SCAN_SETTINGS_DEFAULT { \
   MANUAL_CONTENT_SCAN_METHOD_AUTOMATIC,        /* scan method */ \
   MANUAL_CONTENT_SCAN_SYSTEM_NAME_AUTO,        /* system_name_type */ \
   MANUAL_CONTENT_SCAN_CORE_DETECT,             /* core_type */ \
   MANUAL_CONTENT_SCAN_USE_DB_STRICT, \
   MANUAL_CONTENT_SCAN_SELECT_DB_AUTO, \
   true,                                        /* search_recursively */ \
   true,                                        /* search_archives */ \
   false, \
   false,                                       /* filter_dat_content */ \
   false,                                       /* overwrite_playlist */ \
   false                                        /* validate_entries */ \
}

/* TODO/FIXME - static public global variables */
/* Static settings object
 * > Provides easy access to settings parameters
 *   when creating associated menu entries
 * > We are handling this in almost exactly the same
 *   way as the regular global 'static settings_t *configuration_settings;'
 *   object in retroarch.c. This means it is not inherently thread safe,
 *   but this should not be an issue (i.e. regular configuration_settings
 *   are not thread safe, but we only access them when pushing a
 *   task, not in the task thread itself, so all is well) */
static scan_settings_t scan_settings = MANUAL_CONTENT_SCAN_SETTINGS_DEFAULT;

/* The path and name buffers live outside the struct: three
 * non-zero defaults were dragging eleven kilobytes of zero buffers
 * into the data segment; split, the buffers are bss and the
 * defaults struct is a few dozen bytes. */
static char scan_core_path[PATH_MAX_LENGTH];
static char scan_file_exts_core[PATH_MAX_LENGTH];
static char scan_file_exts_custom[PATH_MAX_LENGTH];
static char scan_dat_file_path[PATH_MAX_LENGTH];
/* A folder or a file */
static char scan_content_dir[PATH_MAX_LENGTH];
static enum manual_content_scan_path_kind scan_content_kind;
/* Number of bytes held back when populating
 * manual_content_scan_task_config_t::system_name, so that the file
 * extensions appended to it downstream (".lpl", ".rdb") still fit in
 * the fixed-size buffers that carry them.  See the comment in
 * manual_content_scan_get_task_config(). */
#define MANUAL_CONTENT_SCAN_SYSTEM_NAME_EXT_RESERVE 4

static char scan_system_name_content_dir[DIR_MAX_LENGTH];
static char scan_system_name_database[NAME_MAX_LENGTH];
static char scan_system_name_custom[NAME_MAX_LENGTH];
static char scan_core_name[NAME_MAX_LENGTH];

/*****************/
/* Configuration */
/*****************/

/* Pointer access */

/* Returns a pointer to the internal
 * 'content_dir' string */
char *manual_content_scan_get_content_dir_ptr(void)
{
   return scan_content_dir;
}

unsigned manual_content_scan_get_scan_method_enum(void)
{
   return scan_settings.scan_method;
}

unsigned manual_content_scan_get_scan_use_db_enum(void)
{
   return scan_settings.db_usage;
}

/* Returns a pointer to the internal
 * 'system_name_custom' string */
char *manual_content_scan_get_system_name_custom_ptr(void)
{
   return scan_system_name_custom;
}

/* Returns size of the internal
 * 'system_name_custom' string */
size_t manual_content_scan_get_system_name_custom_size(void)
{
   return sizeof(scan_system_name_custom);
}

/* Returns a pointer to the internal
 * 'file_exts_custom' string */
char *manual_content_scan_get_file_exts_custom_ptr(void)
{
   return scan_file_exts_custom;
}

/* Returns size of the internal
 * 'file_exts_custom' string */
size_t manual_content_scan_get_file_exts_custom_size(void)
{
   return sizeof(scan_file_exts_custom);
}

/* Returns custom extensions,
 * falling back to core-supported extensions */
const char *manual_content_scan_get_file_exts(void)
{
   if (*scan_file_exts_custom)
      return scan_file_exts_custom;
   return scan_file_exts_core;
}

/* Returns a pointer to the internal
 * 'dat_file_path' string */
char *manual_content_scan_get_dat_file_path_ptr(void)
{
   return scan_dat_file_path;
}

/* Returns size of the internal
 * 'dat_file_path' string */
size_t manual_content_scan_get_dat_file_path_size(void)
{
   return sizeof(scan_dat_file_path);
}

/* Returns a pointer to the internal
 * 'search_recursively' bool */
bool *manual_content_scan_get_search_recursively_ptr(void)
{
   return &scan_settings.search_recursively;
}

/* Returns a pointer to the internal
 * 'search_archives' bool */
bool *manual_content_scan_get_search_archives_ptr(void)
{
   return &scan_settings.search_archives;
}

bool *manual_content_scan_get_omit_db_ref_ptr(void)
{
   return &scan_settings.omit_db_ref;
}

/* Returns a pointer to the internal
 * 'filter_dat_content' bool */
bool *manual_content_scan_get_filter_dat_content_ptr(void)
{
   return &scan_settings.filter_dat_content;
}

/* Returns a pointer to the internal
 * 'overwrite_playlist' bool */
bool *manual_content_scan_get_overwrite_playlist_ptr(void)
{
   return &scan_settings.overwrite_playlist;
}

/* Returns a pointer to the internal
 * 'validate_entries' bool */
bool *manual_content_scan_get_validate_entries_ptr(void)
{
   return &scan_settings.validate_entries;
}

/* Sanitisation */

/* Sanitises file extensions list string:
 * > Removes period (full stop) characters
 * > Converts to lower case
 * > Trims leading/trailing whitespace */
static void manual_content_scan_scrub_file_exts(char *file_exts)
{
   if (!file_exts || !*file_exts)
      return;

   string_remove_all_chars(file_exts, '.');
   string_to_lower(file_exts);
   string_trim_whitespace_right(file_exts);
   string_trim_whitespace_left(file_exts);
}

/* Removes invalid characters from
 * 'system_name_custom' string */
void manual_content_scan_scrub_system_name_custom(void)
{
   char *scrub_char_pointer = NULL;
   if (!*scan_system_name_custom)
      return;
   /* Scrub characters that are not cross-platform
    * and/or violate the No-Intro filename standard:
    * http://datomatic.no-intro.org/stuff/The%20Official%20No-Intro%20Convention%20(20071030).zip
    * Replace these characters with underscores */
   while ((scrub_char_pointer =
            strpbrk(scan_system_name_custom, "&*/:`\"<>?\\|")))
      *scrub_char_pointer = '_';
}

/* Removes period (full stop) characters from
 * 'file_exts_custom' string and converts to
 * lower case */
void manual_content_scan_scrub_file_exts_custom(void)
{
   manual_content_scan_scrub_file_exts(scan_file_exts_custom);
}

/* Performs rudimentary validation of the specified
 * Logiqx XML DAT file path (not rigorous - just enough
 * to prevent obvious errors), optionally returning the
 * file size (DAT files can be very large, so callers
 * use it for free-memory checks).
 * The parser (logiqx_dat.c) is path-agnostic - bytes
 * in, structs out - so both the extension whitelist
 * and the file system checks live here, with the scan
 * configuration that owns the path. */
/* The extensions a Logiqx XML DAT may legitimately carry:
 * .dat and .xml */
static bool manual_content_scan_dat_ext_is_valid(const char *path)
{
   const char *file_ext = NULL;

   if (!path || !*path)
      return false;

   file_ext = path_get_extension(path);

   if (!file_ext || !*file_ext)
      return false;

   return string_is_equal_noncase(file_ext, "dat")
       || string_is_equal_noncase(file_ext, "xml");
}

bool manual_content_scan_dat_path_is_valid(
      const char *path, uint64_t *file_size)
{
   int64_t file_size_int;

   if (!manual_content_scan_dat_ext_is_valid(path))
      return false;

   /* Ensure file exists */
   if (!path_is_valid(path))
      return false;

   /* Get file size */
   file_size_int = path_get_size(path);

   if (file_size_int <= 0)
      return false;

   if (file_size)
      *file_size = (uint64_t)file_size_int;

   return true;
}

/* Checks 'dat_file_path' string and resets it
 * if invalid.  @known_size, when not NULL, is the file's
 * size as read already (<= 0: not there), so the file
 * system is not asked again. */
static enum manual_content_scan_dat_file_path_status
      manual_content_scan_validate_dat(const int64_t *known_size)
{
   enum manual_content_scan_dat_file_path_status dat_file_path_status =
         MANUAL_CONTENT_SCAN_DAT_FILE_UNSET;

   /* Check if 'dat_file_path' has been set */
   if (*scan_dat_file_path)
   {
      uint64_t file_size = 0;
      bool valid;

      /* Check if path itself is valid */
      if (known_size)
      {
         valid = manual_content_scan_dat_ext_is_valid(scan_dat_file_path)
              && *known_size > 0;
         if (valid)
            file_size = (uint64_t)*known_size;
      }
      else
         valid = manual_content_scan_dat_path_is_valid(scan_dat_file_path,
               &file_size);

      if (valid)
      {
         uint64_t free_memory = mem_stats_free();
         dat_file_path_status = MANUAL_CONTENT_SCAN_DAT_FILE_OK;

         /* DAT files can be *very* large...
          * Try to enforce sane behaviour by requiring
          * the system to have an amount of free memory
          * at least twice the size of the DAT file...
          * > Note that desktop (and probably mobile)
          *   platforms should always have enough memory
          *   for this - we're really only protecting the
          *   console ports here */
         if (free_memory > 0)
         {
            if (free_memory < (2 * file_size))
               dat_file_path_status = MANUAL_CONTENT_SCAN_DAT_FILE_TOO_LARGE;
         }
         /* This is an annoying condition - it means the
          * current platform doesn't have a 'free_memory'
          * implementation...
          * Have to make some assumptions in this case:
          * > Typically the lowest system RAM of a supported
          *   platform in 32MB
          * > Propose that (2 * file_size) should be no more
          *   than 1/4 of this total RAM value */
         else if ((2 * file_size) > (8 * 1048576))
            dat_file_path_status = MANUAL_CONTENT_SCAN_DAT_FILE_TOO_LARGE;
      }
      else
         dat_file_path_status = MANUAL_CONTENT_SCAN_DAT_FILE_INVALID;
   }

   /* Reset 'dat_file_path' if status is anything other
    * that 'OK' */
   if (dat_file_path_status != MANUAL_CONTENT_SCAN_DAT_FILE_OK)
      scan_dat_file_path[0] = '\0';

   return dat_file_path_status;
}

enum manual_content_scan_dat_file_path_status
      manual_content_scan_validate_dat_file_path(void)
{
   return manual_content_scan_validate_dat(NULL);
}

/* Menu setters */

/* Sets every setting back to its default */
void manual_content_scan_reset_settings(void)
{
   static const scan_settings_t d = MANUAL_CONTENT_SCAN_SETTINGS_DEFAULT;

   scan_settings                   = d;
   scan_core_path[0]               = '\0';
   scan_file_exts_core[0]          = '\0';
   scan_file_exts_custom[0]        = '\0';
   scan_dat_file_path[0]           = '\0';
   scan_content_dir[0]             = '\0';
   scan_content_kind               = MANUAL_CONTENT_SCAN_PATH_UNKNOWN;
   scan_system_name_content_dir[0] = '\0';
   scan_system_name_database[0]    = '\0';
   scan_system_name_custom[0]      = '\0';
   scan_core_name[0]               = '\0';
}

struct manual_content_scan_saved
{
   scan_settings_t settings;
   enum manual_content_scan_path_kind content_kind;
   char core_path[PATH_MAX_LENGTH];
   char file_exts_core[PATH_MAX_LENGTH];
   char file_exts_custom[PATH_MAX_LENGTH];
   char dat_file_path[PATH_MAX_LENGTH];
   char content_dir[PATH_MAX_LENGTH];
   char system_name_content_dir[DIR_MAX_LENGTH];
   char system_name_database[NAME_MAX_LENGTH];
   char system_name_custom[NAME_MAX_LENGTH];
   char core_name[NAME_MAX_LENGTH];
};

manual_content_scan_saved_t *manual_content_scan_save_settings(void)
{
   manual_content_scan_saved_t *sv = (manual_content_scan_saved_t*)
      malloc(sizeof(*sv));
   if (!sv)
      return NULL;
   sv->settings = scan_settings;
   sv->content_kind = scan_content_kind;
   memcpy(sv->core_path,               scan_core_path,               sizeof(sv->core_path));
   memcpy(sv->file_exts_core,          scan_file_exts_core,          sizeof(sv->file_exts_core));
   memcpy(sv->file_exts_custom,        scan_file_exts_custom,        sizeof(sv->file_exts_custom));
   memcpy(sv->dat_file_path,           scan_dat_file_path,           sizeof(sv->dat_file_path));
   memcpy(sv->content_dir,             scan_content_dir,             sizeof(sv->content_dir));
   memcpy(sv->system_name_content_dir, scan_system_name_content_dir, sizeof(sv->system_name_content_dir));
   memcpy(sv->system_name_database,    scan_system_name_database,    sizeof(sv->system_name_database));
   memcpy(sv->system_name_custom,      scan_system_name_custom,      sizeof(sv->system_name_custom));
   memcpy(sv->core_name,               scan_core_name,               sizeof(sv->core_name));
   return sv;
}

void manual_content_scan_restore_settings(manual_content_scan_saved_t *sv)
{
   if (!sv)
      return;
   scan_settings = sv->settings;
   scan_content_kind = sv->content_kind;
   memcpy(scan_core_path,               sv->core_path,               sizeof(scan_core_path));
   memcpy(scan_file_exts_core,          sv->file_exts_core,          sizeof(scan_file_exts_core));
   memcpy(scan_file_exts_custom,        sv->file_exts_custom,        sizeof(scan_file_exts_custom));
   memcpy(scan_dat_file_path,           sv->dat_file_path,           sizeof(scan_dat_file_path));
   memcpy(scan_content_dir,             sv->content_dir,             sizeof(scan_content_dir));
   memcpy(scan_system_name_content_dir, sv->system_name_content_dir, sizeof(scan_system_name_content_dir));
   memcpy(scan_system_name_database,    sv->system_name_database,    sizeof(scan_system_name_database));
   memcpy(scan_system_name_custom,      sv->system_name_custom,      sizeof(scan_system_name_custom));
   memcpy(scan_core_name,               sv->core_name,               sizeof(scan_core_name));
   free(sv);
}

static enum manual_content_scan_path_kind manual_content_scan_fs_kind(
      const manual_content_scan_fs_t *fs)
{
   if (!fs || !(fs->content_stat & RETRO_VFS_STAT_IS_VALID))
      return MANUAL_CONTENT_SCAN_PATH_UNKNOWN;
   return (fs->content_stat & RETRO_VFS_STAT_IS_DIRECTORY)
      ? MANUAL_CONTENT_SCAN_PATH_FOLDER : MANUAL_CONTENT_SCAN_PATH_FILE;
}

/* Sets content directory for next manual scan
 * operation.
 * Returns true if content directory is valid. */
static bool manual_content_scan_set_content_dir(const char *content_dir,
      const manual_content_scan_fs_t *fs,
      enum manual_content_scan_path_kind kind)
{
   size_t _len;
   int path_stats;
   const char *dir_name = NULL;
   char _tmpbuf[PATH_MAX_LENGTH];

   /* Sanity check before the expansion.  It used to sit after
    * content_dir had been reassigned to _tmpbuf, by which point it
    * could never fire - and fill_pathname_expand_special() had
    * already dereferenced the caller's pointer. */
   if (!content_dir || !*content_dir)
      goto error;

   fill_pathname_expand_special(_tmpbuf, content_dir, sizeof(_tmpbuf));
   content_dir = _tmpbuf;

   if (!*content_dir)
      goto error;

   /* Copy the path to the settings struct and remove a trailing
    * slash.  strlcpy() returns the length of its source, which can
    * exceed the buffer: it is only used as an index once the check
    * below has shown that it fits. */
   _len = strlcpy(scan_content_dir, content_dir,
         sizeof(scan_content_dir));

   if (_len == 0)
      goto error;

   /* A path that did not fit is rejected rather than kept truncated.
    * Truncation cuts mid-component, so what remains is a different
    * directory: scanning ".../subdirectory_number_041/subdirectory_n"
    * either finds nothing, with no indication why, or finds whatever
    * happens to sit at that shortened path.  The tail also becomes
    * the content-directory system name, so a scan that did produce
    * entries would file them under a fragment of a folder name.
    *
    * Nothing is lost by refusing: a path this long already produced
    * one of those two outcomes.  Failing here instead surfaces it as
    * an invalid content directory, which is what the menu already
    * reports for the other rejections above. */
   if (_len >= sizeof(scan_content_dir))
      goto error;

   if (scan_content_dir[_len - 1] == PATH_DEFAULT_SLASH_C())
       scan_content_dir[_len - 1] = '\0';

   /* Handle case where path was a single slash... */
   if (!*scan_content_dir)
      goto error;

   /* Get directory name (used as system name
    * when scan_settings.system_name_type ==
    * MANUAL_CONTENT_SCAN_SYSTEM_NAME_CONTENT_DIR) */
   dir_name = path_basename(scan_content_dir);

   if (!dir_name || !*dir_name)
      goto error;

   scan_content_kind = kind;

   /* A file takes the name of the folder that holds it,
    * as a scan of that folder would */
   path_stats = fs ? fs->content_stat
         : manual_content_scan_content_stat(scan_content_dir);
   if (      (path_stats & RETRO_VFS_STAT_IS_VALID)
         && !(path_stats & RETRO_VFS_STAT_IS_DIRECTORY)
         &&  fill_pathname_parent_dir_name(scan_system_name_content_dir,
               scan_content_dir, sizeof(scan_system_name_content_dir)))
      return true;

   /* Copy directory name to settings struct */
   strlcpy(
         scan_system_name_content_dir,
         dir_name,
         sizeof(scan_system_name_content_dir));

   return true;

error:
   /* Directory is invalid - reset internal
    * content directory and associated 'directory'
    * system name */
   scan_content_dir[0]             = '\0';
   scan_content_kind               = MANUAL_CONTENT_SCAN_PATH_UNKNOWN;
   scan_system_name_content_dir[0] = '\0';
   return false;
}

bool manual_content_scan_set_menu_content_dir(const char *content_dir)
{
   return manual_content_scan_set_content_dir(content_dir, NULL,
         MANUAL_CONTENT_SCAN_PATH_UNKNOWN);
}

bool manual_content_scan_set_menu_content_dir_fs(const char *content_dir,
      const manual_content_scan_fs_t *fs)
{
   return manual_content_scan_set_content_dir(content_dir, fs,
         manual_content_scan_fs_kind(fs));
}

bool manual_content_scan_set_menu_content_dir_kind(const char *content_dir,
      enum manual_content_scan_path_kind kind)
{
   return manual_content_scan_set_content_dir(content_dir, NULL, kind);
}

int manual_content_scan_content_stat(const char *path)
{
   if (scan_content_kind != MANUAL_CONTENT_SCAN_PATH_UNKNOWN)
      return scan_content_kind == MANUAL_CONTENT_SCAN_PATH_FOLDER
         ? RETRO_VFS_STAT_IS_VALID | RETRO_VFS_STAT_IS_DIRECTORY
         : RETRO_VFS_STAT_IS_VALID;
   return path_stat(path);
}

bool manual_content_scan_set_menu_scan_method(
      enum manual_content_scan_method method)
{
   scan_settings.scan_method = method;
   return true;
}

bool manual_content_scan_set_menu_scan_use_db(
      enum manual_content_scan_db_usage usage)
{
   if (usage <= MANUAL_CONTENT_SCAN_USE_DB_NONE)
   {
      scan_settings.db_usage = usage;
      return true;
   }
   else
   {
      scan_settings.db_usage = MANUAL_CONTENT_SCAN_USE_DB_NONE;
      return false;      
   }
}

bool manual_content_scan_set_menu_scan_db_select(
      enum manual_content_scan_db_selection select,
      const char *db_name)
{
   if (select == MANUAL_CONTENT_SCAN_SELECT_DB_AUTO ||
       select == MANUAL_CONTENT_SCAN_SELECT_DB_AUTO_FIRST_MATCH)
   {
      scan_settings.db_selection = select;
      /*scan_system_name_database[0] = '\0';*/
   }
   else
   {
      /* We are using a database name... */
      if (!db_name || !*db_name)
         goto error;

      scan_settings.db_selection = MANUAL_CONTENT_SCAN_SELECT_DB_SPECIFIC;
      /* Copy database name to settings struct */
      strlcpy(
            scan_system_name_database,
            db_name,
            sizeof(scan_system_name_database));
   }
   return true;

error:
   /* Input parameters are invalid - reset internal
    * 'system_name_type' and 'system_name_database' */
   scan_settings.db_selection            = MANUAL_CONTENT_SCAN_SELECT_DB_AUTO;
   scan_system_name_database[0] = '\0';
   return false;
}

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
      const char *system_name)
{
   /* Sanity check */
   if (system_name_type > MANUAL_CONTENT_SCAN_SYSTEM_NAME_DATABASE)
      goto error;

   /* Cache system name 'type' */
   scan_settings.system_name_type = system_name_type;

   /* Check if we are using a non-database name */
   if ((scan_settings.system_name_type == MANUAL_CONTENT_SCAN_SYSTEM_NAME_CONTENT_DIR) ||
       (scan_settings.system_name_type == MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM) ||
       (scan_settings.system_name_type == MANUAL_CONTENT_SCAN_SYSTEM_NAME_AUTO))
      /* No-op. Keep the existing system_name_database. */
      ;
   else
   {
      /* We are using a database name... */
      if (!system_name || !*system_name)
         goto error;

      /* Copy database name to settings struct */
      strlcpy(
            scan_system_name_database,
            system_name,
            sizeof(scan_system_name_database));
   }

   return true;

error:
   /* Input parameters are invalid - reset internal
    * 'system_name_type' and 'system_name_database' */
   scan_settings.system_name_type        = MANUAL_CONTENT_SCAN_SYSTEM_NAME_AUTO;
   scan_system_name_database[0] = '\0';
   return false;
}

/* Copies the core's path, name and extensions
 * to the settings.
 * Returns false if the core has no path. */
static bool manual_content_scan_set_core(const core_info_t *core_info)
{
   /* Copy core path to settings struct */
   if (!core_info->path || !*core_info->path)
      return false;

   strlcpy(
         scan_core_path,
         core_info->path,
         sizeof(scan_core_path));

   /* Copy core name to settings struct */
   strlcpy(
         scan_core_name,
         core_info->display_name,
         sizeof(scan_core_name));

   /* Copy supported extensions to settings
    * struct, if required */
   if (    core_info->supported_extensions
       && *core_info->supported_extensions)
   {
      strlcpy(
            scan_file_exts_core,
            core_info->supported_extensions,
            sizeof(scan_file_exts_core));

      /* Core info extensions are delimited by
       * vertical bars. For internal consistency,
       * replace them with spaces */
      string_replace_all_chars(scan_file_exts_core, '|', ' ');

      /* Apply standard scrubbing/clean-up
       * (should not be required, but must handle the
       * case where a core info file is incorrectly
       * formatted) */
      manual_content_scan_scrub_file_exts(scan_file_exts_core);
   }
   else
      scan_file_exts_core[0] = '\0';

   return true;
}

/* Sets core name for the next manual scan
 * operation (+ core path and other associated
 * parameters).
 * Returns true if core name is valid. */
bool manual_content_scan_set_menu_core_name(
      enum manual_content_scan_core_type core_type,
      const char *core_name)
{
   /* Sanity check */
   if (core_type > MANUAL_CONTENT_SCAN_CORE_SET)
      goto error;

   /* Cache core 'type' */
   scan_settings.core_type = core_type;

   /* Check if we are using core autodetection */
   if (scan_settings.core_type == MANUAL_CONTENT_SCAN_CORE_DETECT)
   {
      scan_core_name[0]      = '\0';
      scan_core_path[0]      = '\0';
      scan_file_exts_core[0] = '\0';
   }
   else
   {
      size_t i;
      core_info_list_t *core_info_list = NULL;
      core_info_t *core_info           = NULL;
      bool core_found                  = false;

      /* We are using a manually set core... */
      if (!core_name || !*core_name)
         goto error;

      /* Get core list */
      core_info_get_list(&core_info_list);

      if (!core_info_list)
         goto error;

      /* Search for the specified core name */
      for (i = 0; i < core_info_list->count; i++)
      {
         core_info = NULL;
         core_info = core_info_get(core_info_list, i);

         if (core_info)
         {
            if (string_is_equal(core_info->display_name, core_name))
            {
               /* Core has been found */
               core_found = true;

               if (!manual_content_scan_set_core(core_info))
                  goto error;

               break;
            }
         }
      }

      /* Sanity check */
      if (!core_found)
         goto error;
   }

   return true;

error:
   /* Input parameters are invalid - reset internal
    * core values */
   scan_settings.core_type         = MANUAL_CONTENT_SCAN_CORE_DETECT;
   scan_core_name[0]      = '\0';
   scan_core_path[0]      = '\0';
   scan_file_exts_core[0] = '\0';
   return false;
}

/* Sets all parameters for the next manual scan
 * operation according the to recorded values in
 * the specified playlist.
 * Returns MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_OK
 * if playlist contains a valid scan record. */
enum manual_content_scan_playlist_refresh_status
      manual_content_scan_set_menu_from_playlist_list(playlist_t *playlist,
            const struct string_list *rdb_list)
{
   return manual_content_scan_set_menu_from_playlist_list_fs(playlist,
         rdb_list, NULL);
}

enum manual_content_scan_playlist_refresh_status
      manual_content_scan_set_menu_from_playlist_list_fs(
            playlist_t *playlist, const struct string_list *rdb_list,
            const manual_content_scan_fs_t *fs)
{
   const char *playlist_path    = NULL;
   const char *content_dir      = NULL;
   const char *core_name        = NULL;
   const char *file_exts        = NULL;
   const char *dat_file_path    = NULL;
   const char *database_name_lpl = NULL;
   bool search_recursively      = false;
   bool search_archives         = false;
   bool filter_dat_content      = false;
   bool overwrite_playlist      = false;
   bool omit_db_ref             = false;
   int db_usage                 = MANUAL_CONTENT_SCAN_USE_DB_NONE;
   enum manual_content_scan_system_name_type
         system_name_type       = MANUAL_CONTENT_SCAN_SYSTEM_NAME_CONTENT_DIR;
   enum manual_content_scan_core_type
         core_type              = MANUAL_CONTENT_SCAN_CORE_DETECT;
   char system_name[NAME_MAX_LENGTH];
   char database_name[NAME_MAX_LENGTH];

   if (!playlist_scan_refresh_enabled(playlist))
      return MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_MISSING_CONFIG;

   /* Read scan parameters from playlist */
   playlist_path      = playlist_get_conf_path(playlist);
   content_dir        = playlist_get_scan_content_dir(playlist);
   core_name          = playlist_get_default_core_name(playlist);
   file_exts          = playlist_get_scan_file_exts(playlist);
   dat_file_path      = playlist_get_scan_dat_file_path(playlist);
   database_name_lpl  = playlist_get_scan_database_name(playlist);

   search_recursively = playlist_get_scan_search_recursively(playlist);
   search_archives    = playlist_get_scan_search_archives(playlist);
   filter_dat_content = playlist_get_scan_filter_dat_content(playlist);
   overwrite_playlist = playlist_get_scan_overwrite_playlist(playlist);
   omit_db_ref        = playlist_get_scan_omit_db_ref(playlist);
   db_usage           = playlist_get_scan_db_usage(playlist);

   /* Determine system name (playlist basename
    * without extension) */
   /* Cannot happen, but would constitute a
    * 'system name' error */
   if (!playlist_path || !*playlist_path)
      return MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_INVALID_SYSTEM_NAME;

   fill_pathname(system_name, path_basename(playlist_path),
         "", sizeof(system_name));

   if (database_name_lpl && *database_name_lpl)
      fill_pathname(database_name,database_name_lpl,"",sizeof(database_name));
   else
      database_name[0] = '\0';
   /* Cannot happen, but would constitute a
    * 'system name' error */
   if (!*system_name)
      return MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_INVALID_SYSTEM_NAME;

   /* Set content directory: a file scan saves none */
   if (!manual_content_scan_set_content_dir(content_dir, fs,
         fs ? manual_content_scan_fs_kind(fs)
         : MANUAL_CONTENT_SCAN_PATH_FOLDER))
      return MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_INVALID_CONTENT_DIR;

   if (!manual_content_scan_set_menu_scan_use_db((enum manual_content_scan_db_usage)db_usage))
      return MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_MISSING_CONFIG;

   scan_settings.db_selection = MANUAL_CONTENT_SCAN_SELECT_DB_AUTO;

   /* Set system name */
   if (rdb_list && rdb_list->size)
   {
      size_t i;

      /* Loop over database files */
      for (i = 0; i < rdb_list->size; i++)
      {
         char rdb_name[NAME_MAX_LENGTH];
         const char *rdb_path = rdb_list->elems[i].data;
         /* Sanity check */
         if (!rdb_path || !*rdb_path)
            continue;
         fill_pathname(rdb_name, path_basename(rdb_path), "",
               sizeof(rdb_name));
         if (!*rdb_name)
            continue;
         /* Check whether playlist system name
          * matches current database file */
         if (string_is_equal(system_name, rdb_name))
         {
            system_name_type = MANUAL_CONTENT_SCAN_SYSTEM_NAME_DATABASE;
            strlcpy(scan_system_name_database, system_name,
               sizeof(scan_system_name_database));
            scan_settings.db_selection = MANUAL_CONTENT_SCAN_SELECT_DB_SPECIFIC;
            break;
         }
         if (string_is_equal(database_name, rdb_name))
         {
            strlcpy(scan_system_name_database, database_name,
               sizeof(scan_system_name_database));
            scan_settings.db_selection = MANUAL_CONTENT_SCAN_SELECT_DB_SPECIFIC;
         }
      }
   }

   /* > If system name does not match a database
    *   file, then check whether it matches the
    *   content directory name */
   if (system_name_type !=
         MANUAL_CONTENT_SCAN_SYSTEM_NAME_DATABASE)
   {
      /* system_name_type is set to
       * MANUAL_CONTENT_SCAN_SYSTEM_NAME_CONTENT_DIR
       * by default - so if a match is found just
       * reset 'custom name' field */
      if (string_is_equal(system_name,
            scan_system_name_content_dir))
         scan_system_name_custom[0] = '\0';
      else
      {
         /* Playlist is using a custom system name */
         system_name_type = MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM;
         strlcpy(scan_system_name_custom, system_name,
               sizeof(scan_system_name_custom));
      }
   }

   if (!manual_content_scan_set_menu_system_name(
         system_name_type, system_name))
      return MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_INVALID_SYSTEM_NAME;

   /* Set core path/name */
   if (   (core_name && *core_name)
       && !string_is_equal(core_name, FILE_PATH_DETECT))
      core_type = MANUAL_CONTENT_SCAN_CORE_SET;

   if (!manual_content_scan_set_menu_core_name(
         core_type, core_name))
      return MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_INVALID_CORE;

   /* Set custom file extensions */
   if (!file_exts || !*file_exts)
      scan_file_exts_custom[0] = '\0';
   else
   {
      strlcpy(scan_file_exts_custom, file_exts,
            sizeof(scan_file_exts_custom));

      /* File extensions read from playlist should
       * be correctly formatted, with '|' characters
       * as delimiters
       * > For menu purposes, must replace these
       *   delimiters with space characters
       * > Additionally scrub the resultant string,
       *   to handle the case where a user has
       *   'corrupted' it by manually tampering with
       *   the playlist file */
      string_replace_all_chars(scan_file_exts_custom, '|', ' ');
      manual_content_scan_scrub_file_exts(scan_file_exts_custom);
   }

   /* Set DAT file path */
   if (!dat_file_path || !*dat_file_path)
      scan_dat_file_path[0] = '\0';
   else
   {
      fill_pathname_expand_special(scan_dat_file_path, dat_file_path,
            sizeof(scan_dat_file_path));

      switch (manual_content_scan_validate_dat(fs ? &fs->dat_size : NULL))
      {
         case MANUAL_CONTENT_SCAN_DAT_FILE_INVALID:
            return MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_INVALID_DAT_FILE;
         case MANUAL_CONTENT_SCAN_DAT_FILE_TOO_LARGE:
            return MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_DAT_FILE_TOO_LARGE;
         default:
            /* No action required */
            break;
      }
   }

   /* Set remaining boolean parameters */
   scan_settings.search_recursively = search_recursively;
   scan_settings.search_archives    = search_archives;
   scan_settings.filter_dat_content = filter_dat_content;
   scan_settings.overwrite_playlist = overwrite_playlist;
   scan_settings.omit_db_ref        = omit_db_ref;
   scan_settings.scan_method        = MANUAL_CONTENT_SCAN_METHOD_CUSTOM;
   /* When refreshing a playlist:
    * > We always validate entries in the
    *   existing file */
   scan_settings.validate_entries   = true;

   return MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_OK;
}

enum manual_content_scan_playlist_refresh_status
      manual_content_scan_set_menu_from_playlist(playlist_t *playlist,
            const char *path_content_database, bool show_hidden_files)
{
   enum manual_content_scan_playlist_refresh_status status;
   struct string_list *rdb_list = NULL;
#ifdef HAVE_LIBRETRODB
   /* > If platform has database support, get names
    *   of all installed database files */
   rdb_list = dir_list_new_special(
         path_content_database,
         DIR_LIST_DATABASES, NULL, show_hidden_files);
#endif
   status   = manual_content_scan_set_menu_from_playlist_list(playlist,
         rdb_list);
   string_list_free(rdb_list);
   return status;
}

/* Whether @name is an installed database: the basename,
 * without extension, of a path in @rdb_list */
static bool manual_content_scan_rdb_list_has(
      const struct string_list *rdb_list, const char *name)
{
   size_t i;

   if (!rdb_list)
      return false;

   for (i = 0; i < rdb_list->size; i++)
   {
      char rdb_name[NAME_MAX_LENGTH];
      const char *rdb_path = rdb_list->elems[i].data;
      if (!rdb_path || !*rdb_path)
         continue;
      fill_pathname(rdb_name, path_basename(rdb_path), "",
            sizeof(rdb_name));
      if (string_is_equal(name, rdb_name))
         return true;
   }

   return false;
}

enum manual_content_scan_core_key
{
   MANUAL_CONTENT_SCAN_CORE_KEY_PATH = 0,
   MANUAL_CONTENT_SCAN_CORE_KEY_FILE_ID_EXACT,
   MANUAL_CONTENT_SCAN_CORE_KEY_FILE_ID,
   MANUAL_CONTENT_SCAN_CORE_KEY_DISPLAY_NAME
};

/* Counts the cores whose @key is @s; *found gets the first */
static unsigned manual_content_scan_match_cores(
      core_info_list_t *core_info_list,
      enum manual_content_scan_core_key key, const char *s,
      const core_info_t **found)
{
   size_t i;
   unsigned count = 0;
#ifdef _WIN32
   /* Either slash: core_info joins its folder and file with '/' */
   char *core_path = NULL;

   if (   key == MANUAL_CONTENT_SCAN_CORE_KEY_PATH
       && !(core_path = (char*)malloc(PATH_MAX_LENGTH)))
      return 0;
#endif

   for (i = 0; i < core_info_list->count; i++)
   {
      const core_info_t *core_info = core_info_get(core_info_list, i);
      bool match                   = false;

      if (!core_info)
         continue;

      switch (key)
      {
         case MANUAL_CONTENT_SCAN_CORE_KEY_PATH:
#ifdef _WIN32
            if (core_info->path)
            {
               strlcpy(core_path, core_info->path, PATH_MAX_LENGTH);
               pathname_conform_slashes_to_os(core_path);
               match = string_is_equal_case_insensitive(core_path, s);
            }
#else
            match = string_is_equal(core_info->path, s);
#endif
            break;
         case MANUAL_CONTENT_SCAN_CORE_KEY_FILE_ID_EXACT:
            match = string_is_equal(core_info->core_file_id.str, s);
            break;
         case MANUAL_CONTENT_SCAN_CORE_KEY_FILE_ID:
            match = string_is_equal_case_insensitive(
                  core_info->core_file_id.str, s);
            break;
         case MANUAL_CONTENT_SCAN_CORE_KEY_DISPLAY_NAME:
            match = string_is_equal(core_info->display_name, s);
            break;
      }

      if (match && !count++)
         *found = core_info;
   }

#ifdef _WIN32
   free(core_path);
#endif
   return count;
}

/* Whether @core is written as a path: display names hold
 * slashes too, so a path that matches no core falls through
 * to the names */
static bool manual_content_scan_core_is_path(const char *core)
{
   return strchr(core, '/')
#ifdef _WIN32
       || strchr(core, '\\')
#endif
       || *core == '~';
}

/* @core, a path, as the core list holds paths: expanded, and
 * "//" and ".." folded in an absolute path too; "" when it
 * resolves to nothing.  Asks the file system. */
static void manual_content_scan_core_path_resolve(char *s,
      const char *core, size_t len)
{
   fill_pathname_expand_special(s, core, len);
#ifndef RARCH_CONSOLE
   if (   *s
       && !path_resolve_realpath(s, len, false)
       && !path_is_absolute(s))
      *s = '\0';
#endif
#ifdef _WIN32
   pathname_conform_slashes_to_os(s);
#endif
}

/* An override DAT path as the scan opens it: expanded, and a
 * relative one resolved; "" when it resolves to nothing.
 * Asks the file system. */
static void manual_content_scan_dat_path_resolve(char *s,
      const char *dat, size_t len)
{
   fill_pathname_expand_special(s, dat, len);
#ifndef RARCH_CONSOLE
   if (   *s && !path_is_absolute(s)
       && !path_resolve_realpath(s, len, false))
      *s = '\0';
#endif
}

/* Finds the core @core names: a path, else a file name or
 * file id, else a display name. The first of these that
 * matches decides, and *count gets its matches.
 * > @resolved: @core resolved as a path already, or NULL to
 *   resolve it here */
static const core_info_t *manual_content_scan_find_core(
      const char *core, const char *resolved, unsigned *count)
{
   core_info_list_t *core_info_list = NULL;
   const core_info_t *found         = NULL;
   char *s                          = NULL;
   char *lower                      = NULL;

   *count = 0;
   core_info_get_list(&core_info_list);

   if (!core_info_list || !(s = (char*)malloc(PATH_MAX_LENGTH)))
      return NULL;

   if (manual_content_scan_core_is_path(core))
   {
      if (resolved)
         strlcpy(s, resolved, PATH_MAX_LENGTH);
      else
         manual_content_scan_core_path_resolve(s, core, PATH_MAX_LENGTH);
      if (*s)
         *count = manual_content_scan_match_cores(core_info_list,
               MANUAL_CONTENT_SCAN_CORE_KEY_PATH, s, &found);
   }

   /* Lower case, as core_info finds "_libretro" in lower case only */
   if (!*count && (lower = strdup(core)))
   {
      size_t i, _len;
      string_to_lower(lower);
      /* core_info's rule would cut a bare id's last "_" part */
      if (strstr(lower, "_libretro"))
         _len = core_info_get_file_id(lower, s, PATH_MAX_LENGTH);
      else
      {
         _len = fill_pathname(s, lower, "", PATH_MAX_LENGTH);
         strlcpy(s + _len, "_libretro", PATH_MAX_LENGTH - _len);
      }
      /* The case typed, for an exact match first */
      for (i = 0; i < _len; i++)
         if (s[i] == lower[i])
            s[i] = core[i];
      free(lower);
      *count = manual_content_scan_match_cores(core_info_list,
            MANUAL_CONTENT_SCAN_CORE_KEY_FILE_ID_EXACT, s, &found);
      if (!*count)
         *count = manual_content_scan_match_cores(core_info_list,
               MANUAL_CONTENT_SCAN_CORE_KEY_FILE_ID, s, &found);
   }

   if (!*count)
      *count = manual_content_scan_match_cores(core_info_list,
            MANUAL_CONTENT_SCAN_CORE_KEY_DISPLAY_NAME, core, &found);

   free(s);
   return found;
}

manual_content_scan_overrides_t *manual_content_scan_overrides_new(void)
{
   return (manual_content_scan_overrides_t*)calloc(1,
         sizeof(manual_content_scan_overrides_t));
}

void manual_content_scan_overrides_free(manual_content_scan_overrides_t *ov)
{
   if (!ov)
      return;
   free(ov->playlist);
   free(ov->database);
   free(ov->dat);
   free(ov->core);
   free(ov->exts);
   free(ov);
}

manual_content_scan_fs_t *manual_content_scan_fs_read(
      const char *content_dir, const char *dat,
      const char *ov_core, const char *ov_dat)
{
   manual_content_scan_fs_t *fs = (manual_content_scan_fs_t*)calloc(1,
         sizeof(*fs));
   char *s                      = (char*)malloc(PATH_MAX_LENGTH);

   if (!fs || !s)
      goto error;

   /* The folder as manual_content_scan_set_menu_content_dir()
    * stats it: expanded, one trailing slash gone */
   if (content_dir && *content_dir)
   {
      size_t _len = fill_pathname_expand_special(s, content_dir,
            PATH_MAX_LENGTH);
      if (_len > 0 && _len < PATH_MAX_LENGTH
            && s[_len - 1] == PATH_DEFAULT_SLASH_C())
         s[_len - 1] = '\0';
      if (*s)
         fs->content_stat = path_stat(s);
   }

   /* The saved DAT, as a refresh expands it */
   fs->dat_size = -1;
   if (dat && *dat)
   {
      fill_pathname_expand_special(s, dat, PATH_MAX_LENGTH);
      if (*s && path_is_valid(s))
         fs->dat_size = path_get_size(s);
   }

   fs->ov_dat_size = -1;
   if (ov_dat)
   {
      manual_content_scan_dat_path_resolve(s, ov_dat, PATH_MAX_LENGTH);
      if (!(fs->ov_dat = strdup(s)))
         goto error;
      if (*s && path_is_valid(s))
         fs->ov_dat_size = path_get_size(s);
   }

   if (ov_core && manual_content_scan_core_is_path(ov_core))
   {
      manual_content_scan_core_path_resolve(s, ov_core, PATH_MAX_LENGTH);
      if (!(fs->ov_core = strdup(s)))
         goto error;
   }

   free(s);
   return fs;

error:
   free(s);
   manual_content_scan_fs_free(fs);
   return NULL;
}

void manual_content_scan_fs_free(manual_content_scan_fs_t *fs)
{
   if (!fs)
      return;
   free(fs->ov_dat);
   free(fs->ov_core);
   free(fs);
}

/* Lays @ov over the current settings and sets the Custom
 * method, stopping at the first override that does not
 * resolve; NULL @ov changes nothing.
 * > @rdb_list: paths of the installed databases, or NULL
 * > @count: the cores @ov->core matched, when not NULL */
enum manual_content_scan_override_status
      manual_content_scan_apply_overrides(
            const manual_content_scan_overrides_t *ov,
            const struct string_list *rdb_list, unsigned *count)
{
   return manual_content_scan_apply_overrides_fs(ov, rdb_list, count,
         NULL);
}

enum manual_content_scan_override_status
      manual_content_scan_apply_overrides_fs(
            const manual_content_scan_overrides_t *ov,
            const struct string_list *rdb_list, unsigned *count,
            const manual_content_scan_fs_t *fs)
{
   if (count)
      *count = 0;

   if (!ov)
      return MANUAL_CONTENT_SCAN_OVR_OK;

   if (ov->core)
   {
      unsigned matches             = 0;
      const core_info_t *core_info = manual_content_scan_find_core(
            ov->core, fs ? fs->ov_core : NULL, &matches);

      if (count)
         *count = matches;
      if (matches > 1)
         return MANUAL_CONTENT_SCAN_OVR_AMBIGUOUS_CORE;
      if (!core_info || !manual_content_scan_set_core(core_info))
         return MANUAL_CONTENT_SCAN_OVR_NO_SUCH_CORE;
      scan_settings.core_type = MANUAL_CONTENT_SCAN_CORE_SET;
   }

   if (ov->database)
   {
      if (!manual_content_scan_rdb_list_has(rdb_list, ov->database))
         return MANUAL_CONTENT_SCAN_OVR_NO_SUCH_DATABASE;
      /* A refreshed database playlist's name shares this
       * buffer: it keeps that name as a custom one */
      if (   scan_settings.system_name_type
               == MANUAL_CONTENT_SCAN_SYSTEM_NAME_DATABASE
          && !string_is_equal(scan_system_name_database, ov->database))
      {
         strlcpy(scan_system_name_custom, scan_system_name_database,
               sizeof(scan_system_name_custom));
         manual_content_scan_set_menu_system_name(
               MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM, NULL);
      }
      manual_content_scan_set_menu_scan_db_select(
            MANUAL_CONTENT_SCAN_SELECT_DB_SPECIFIC, ov->database);
   }
   else if (ov->flags & MANUAL_CONTENT_SCAN_OVR_FIRST_DB)
      manual_content_scan_set_menu_scan_db_select(
            MANUAL_CONTENT_SCAN_SELECT_DB_AUTO_FIRST_MATCH, NULL);

   /* A database playlist's name and the database to match
    * share one buffer: next to another database, the
    * playlist is custom */
   if (ov->playlist)
   {
      if (   manual_content_scan_rdb_list_has(rdb_list, ov->playlist)
          && (!ov->database || string_is_equal(ov->database, ov->playlist)))
         manual_content_scan_set_menu_system_name(
               MANUAL_CONTENT_SCAN_SYSTEM_NAME_DATABASE, ov->playlist);
      else
      {
         strlcpy(scan_system_name_custom, ov->playlist,
               sizeof(scan_system_name_custom));
         manual_content_scan_scrub_system_name_custom();
         manual_content_scan_set_menu_system_name(
               MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM, NULL);
      }
   }

   if (ov->dat)
   {
      if (   (ov->flags & MANUAL_CONTENT_SCAN_OVR_MATCH)
          && ov->match == MANUAL_CONTENT_SCAN_USE_DB_NONE)
         return MANUAL_CONTENT_SCAN_OVR_DAT_WITHOUT_MATCH;

      if (fs && fs->ov_dat)
         strlcpy(scan_dat_file_path, fs->ov_dat,
               sizeof(scan_dat_file_path));
      else
         manual_content_scan_dat_path_resolve(scan_dat_file_path, ov->dat,
               sizeof(scan_dat_file_path));
      switch (manual_content_scan_validate_dat(
            (fs && fs->ov_dat) ? &fs->ov_dat_size : NULL))
      {
         case MANUAL_CONTENT_SCAN_DAT_FILE_OK:
            break;
         case MANUAL_CONTENT_SCAN_DAT_FILE_TOO_LARGE:
            return MANUAL_CONTENT_SCAN_OVR_DAT_TOO_LARGE;
         default:
            return MANUAL_CONTENT_SCAN_OVR_NOT_A_DAT;
      }

      manual_content_scan_set_menu_scan_use_db(
               (   (ov->flags & MANUAL_CONTENT_SCAN_OVR_MATCH)
                && ov->match == MANUAL_CONTENT_SCAN_USE_DB_LOOSE)
            ? MANUAL_CONTENT_SCAN_USE_DB_DAT_LOOSE
            : MANUAL_CONTENT_SCAN_USE_DB_DAT_STRICT);
   }
   else if (ov->flags & MANUAL_CONTENT_SCAN_OVR_MATCH)
   {
      enum manual_content_scan_db_usage usage = ov->match;

      /* A refresh keeps its saved DAT unless the match is none */
      if (   scan_settings.db_usage == MANUAL_CONTENT_SCAN_USE_DB_DAT_STRICT
          || scan_settings.db_usage == MANUAL_CONTENT_SCAN_USE_DB_DAT_LOOSE)
      {
         if (ov->match == MANUAL_CONTENT_SCAN_USE_DB_STRICT)
            usage = MANUAL_CONTENT_SCAN_USE_DB_DAT_STRICT;
         else if (ov->match == MANUAL_CONTENT_SCAN_USE_DB_LOOSE)
            usage = MANUAL_CONTENT_SCAN_USE_DB_DAT_LOOSE;
         else
            scan_dat_file_path[0] = '\0';
      }

      manual_content_scan_set_menu_scan_use_db(usage);
   }

   if (ov->exts)
   {
      strlcpy(scan_file_exts_custom, ov->exts,
            sizeof(scan_file_exts_custom));
      manual_content_scan_scrub_file_exts_custom();
   }

   if (ov->flags & MANUAL_CONTENT_SCAN_OVR_NO_RECURSIVE)
      scan_settings.search_recursively = false;
   if (ov->flags & MANUAL_CONTENT_SCAN_OVR_NO_ARCHIVES)
      scan_settings.search_archives    = false;
   if (ov->flags & MANUAL_CONTENT_SCAN_OVR_OVERWRITE)
      scan_settings.overwrite_playlist = true;
   if (ov->flags & MANUAL_CONTENT_SCAN_OVR_VALIDATE)
      scan_settings.validate_entries   = true;
   if (ov->flags & MANUAL_CONTENT_SCAN_OVR_OMIT_DB_REF)
      scan_settings.omit_db_ref        = true;

   manual_content_scan_set_menu_scan_method(
         MANUAL_CONTENT_SCAN_METHOD_CUSTOM);

   return MANUAL_CONTENT_SCAN_OVR_OK;
}

/* Menu getters */

/* Fetches content directory for next manual scan
 * operation.
 * Returns true if content directory is valid. */
bool manual_content_scan_get_menu_content_dir(const char **content_dir)
{
   if (!content_dir)
      return false;
   if (!*scan_content_dir)
      return false;
   *content_dir = scan_content_dir;
   return true;
}

bool manual_content_scan_get_menu_scan_method(const char **scan_method)
{
   if (scan_method)
   {
      switch (scan_settings.scan_method)
      {
         case MANUAL_CONTENT_SCAN_METHOD_AUTOMATIC:
            *scan_method = msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_SCAN_METHOD_AUTO);
            return true;
         case MANUAL_CONTENT_SCAN_METHOD_CUSTOM:
            *scan_method = msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_SCAN_METHOD_CUSTOM);
            return true;
         default:
            break;
      }
   }
   return false;
}

bool manual_content_scan_get_menu_scan_use_db(const char **scan_use_db)
{
   if (scan_use_db)
   {
      switch (scan_settings.db_usage)
      {
         case MANUAL_CONTENT_SCAN_USE_DB_STRICT:
            *scan_use_db = msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_SCAN_USE_DB_STRICT);
            return true;
         case MANUAL_CONTENT_SCAN_USE_DB_LOOSE:
            *scan_use_db = msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_SCAN_USE_DB_LOOSE);
            return true;
         case MANUAL_CONTENT_SCAN_USE_DB_DAT_STRICT:
            *scan_use_db = msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_SCAN_USE_DB_CUSTOM_DAT);
            return true;
         case MANUAL_CONTENT_SCAN_USE_DB_DAT_LOOSE:
            *scan_use_db = msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_SCAN_USE_DB_CUSTOM_DAT_LOOSE);
            return true;
         case MANUAL_CONTENT_SCAN_USE_DB_NONE:
            *scan_use_db = msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_SCAN_USE_DB_NONE);
            return true;
         default:
            break;
      }
   }
   return false;
}

unsigned manual_content_scan_get_scan_db_select_enum(void)
{
   return scan_settings.db_selection;
}

bool manual_content_scan_get_menu_scan_db_select(const char **scan_db_select)
{
   if (scan_db_select)
   {
      switch (scan_settings.db_selection)
      {
         case MANUAL_CONTENT_SCAN_SELECT_DB_AUTO:
            *scan_db_select = msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_SCAN_DB_SELECT_AUTO_ANY);
            return true;
         case MANUAL_CONTENT_SCAN_SELECT_DB_AUTO_FIRST_MATCH:
            *scan_db_select = msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_SCAN_DB_SELECT_AUTO_FIRST);
            return true;
         case MANUAL_CONTENT_SCAN_SELECT_DB_SPECIFIC:
            if (*scan_system_name_database)
            {
               *scan_db_select = scan_system_name_database;
               return true;
            }
            break;
         default:
            break;
      }
   }
   return false;
}

/* Fetches system name for the next manual scan operation.
 * Returns true if system name is valid.
 * NOTE: This corresponds to the 'System Name' value
 * displayed in menus - this is not identical to the
 * actual system name used when generating the playlist */
bool manual_content_scan_get_menu_system_name(const char **system_name)
{
   if (system_name)
   {
      switch (scan_settings.system_name_type)
      {
         case MANUAL_CONTENT_SCAN_SYSTEM_NAME_CONTENT_DIR:
            *system_name = msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CONTENT_DIR);
            return true;
         case MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM:
            *system_name = msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CUSTOM);
            return true;
         case MANUAL_CONTENT_SCAN_SYSTEM_NAME_AUTO:
            *system_name = msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_AUTO);
            return true;

         case MANUAL_CONTENT_SCAN_SYSTEM_NAME_DATABASE:
            if (*scan_system_name_database)
            {
               *system_name = scan_system_name_database;
               return true;
            }
            break;
         default:
            break;
      }
   }

   return false;
}

unsigned manual_content_scan_get_menu_system_name_type(void)
{
   return scan_settings.system_name_type;
}
/* Fetches core name for the next manual scan operation.
 * Returns true if core name is valid. */
bool manual_content_scan_get_menu_core_name(const char **core_name)
{
   if (core_name)
   {
      switch (scan_settings.core_type)
      {
         case MANUAL_CONTENT_SCAN_CORE_DETECT:
            *core_name = msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME_DETECT);
            return true;
         case MANUAL_CONTENT_SCAN_CORE_SET:
            if (*scan_core_name)
            {
               *core_name = scan_core_name;
               return true;
            }
            break;
         default:
            break;
      }
   }

   return false;
}

/* Menu utility functions */


struct string_list *manual_content_scan_get_menu_scan_method_list(void)
{
   union string_list_elem_attr attr;
   struct string_list *name_list = string_list_new();

   /* Sanity check */
   if (!name_list)
      return NULL;

   attr.i = 0;

   if (!string_list_append(name_list, msg_hash_to_str(
         MENU_ENUM_LABEL_VALUE_SCAN_METHOD_AUTO), attr))
      goto error;

   if (!string_list_append(name_list, msg_hash_to_str(
         MENU_ENUM_LABEL_VALUE_SCAN_METHOD_CUSTOM), attr))
      goto error;

   return name_list;

error:
   if (name_list)
      string_list_free(name_list);
   return NULL;

}

struct string_list *manual_content_scan_get_menu_scan_use_db_list(void)
{
   union string_list_elem_attr attr;
   struct string_list *name_list = string_list_new();

   /* Sanity check */
   if (!name_list)
      return NULL;

   attr.i = 0;

   if (!string_list_append(name_list, msg_hash_to_str(
         MENU_ENUM_LABEL_VALUE_SCAN_USE_DB_STRICT), attr))
      goto error;

   if (!string_list_append(name_list, msg_hash_to_str(
         MENU_ENUM_LABEL_VALUE_SCAN_USE_DB_LOOSE), attr))
      goto error;

   if (!string_list_append(name_list, msg_hash_to_str(
         MENU_ENUM_LABEL_VALUE_SCAN_USE_DB_CUSTOM_DAT), attr))
      goto error;

   if (!string_list_append(name_list, msg_hash_to_str(
         MENU_ENUM_LABEL_VALUE_SCAN_USE_DB_CUSTOM_DAT_LOOSE), attr))
      goto error;

   if (!string_list_append(name_list, msg_hash_to_str(
         MENU_ENUM_LABEL_VALUE_SCAN_USE_DB_NONE), attr))
      goto error;

   return name_list;

error:
   if (name_list)
      string_list_free(name_list);
   return NULL;

}


struct string_list *manual_content_scan_get_menu_scan_db_select_list(
               const char *path_content_database, bool show_hidden_files)
{
   union string_list_elem_attr attr;
   struct string_list *name_list = string_list_new();

   /* Sanity check */
   if (!name_list)
      return NULL;

   attr.i = 0;

   /* Add 'use content directory' entry */
   if (!string_list_append(name_list, msg_hash_to_str(
         MENU_ENUM_LABEL_VALUE_SCAN_DB_SELECT_AUTO_ANY), attr))
      goto error;

   /* Add 'use custom' entry */
   if (!string_list_append(name_list, msg_hash_to_str(
         MENU_ENUM_LABEL_VALUE_SCAN_DB_SELECT_AUTO_FIRST), attr))
      goto error;

#ifdef HAVE_LIBRETRODB
   /* If platform has database support, get names
    * of all installed database files */
   {
      /* Note: dir_list_new_special() is well behaved - the
       * returned string list will only include database
       * files (i.e. don't have to check for directories,
       * or verify file extensions) */
      struct string_list *rdb_list = dir_list_new_special(
            path_content_database,
            DIR_LIST_DATABASES, NULL, show_hidden_files);

      if (rdb_list && rdb_list->size)
      {
         unsigned i;

         /* Ensure database list is in alphabetical order */
         dir_list_sort(rdb_list, true);

         /* Loop over database files */
         for (i = 0; i < rdb_list->size; i++)
         {
            char rdb_name[NAME_MAX_LENGTH];
            const char *rdb_path = rdb_list->elems[i].data;
            /* Sanity check */
            if (!rdb_path || !*rdb_path)
               continue;
            fill_pathname(rdb_name, path_basename(rdb_path), "",
                  sizeof(rdb_name));
            if (!*rdb_name)
               continue;
            /* Add database name to list */
            if (!string_list_append(name_list, rdb_name, attr))
               goto error;
         }
      }

      /* Clean up */
      string_list_free(rdb_list);
   }

#endif

   return name_list;

error:
   if (name_list)
      string_list_free(name_list);
   return NULL;
}


/* Creates a list of all possible 'system name' menu
 * strings, for use in 'menu_displaylist' drop-down
 * lists and 'menu_cbs_left/right'
 * > Returns NULL in the event of failure
 * > Returned string list must be free()'d */
struct string_list *manual_content_scan_get_menu_system_name_list(
      const char *path_content_database, bool show_hidden_files)
{
   union string_list_elem_attr attr;
   struct string_list *name_list = string_list_new();

   /* Sanity check */
   if (!name_list)
      return NULL;

   attr.i = 0;

   /* Add 'use content directory' entry */
   if (!string_list_append(name_list, msg_hash_to_str(
         MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CONTENT_DIR), attr))
      goto error;

   /* Add 'use custom' entry */
   if (!string_list_append(name_list, msg_hash_to_str(
         MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CUSTOM), attr))
      goto error;

   /* Add 'use custom' entry */
   if (!string_list_append(name_list, msg_hash_to_str(
         MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_AUTO), attr))
      goto error;

#ifdef HAVE_LIBRETRODB
   /* If platform has database support, get names
    * of all installed database files */
   {
      /* Note: dir_list_new_special() is well behaved - the
       * returned string list will only include database
       * files (i.e. don't have to check for directories,
       * or verify file extensions) */
      struct string_list *rdb_list = dir_list_new_special(
            path_content_database,
            DIR_LIST_DATABASES, NULL, show_hidden_files);

      if (rdb_list && rdb_list->size)
      {
         unsigned i;

         /* Ensure database list is in alphabetical order */
         dir_list_sort(rdb_list, true);

         /* Loop over database files */
         for (i = 0; i < rdb_list->size; i++)
         {
            char rdb_name[NAME_MAX_LENGTH];
            const char *rdb_path = rdb_list->elems[i].data;
            /* Sanity check */
            if (!rdb_path || !*rdb_path)
               continue;
            fill_pathname(rdb_name, path_basename(rdb_path), "",
                  sizeof(rdb_name));
            if (!*rdb_name)
               continue;
            /* Add database name to list */
            if (!string_list_append(name_list, rdb_name, attr))
               goto error;
         }
      }

      /* Clean up */
      string_list_free(rdb_list);
   }

#endif

   return name_list;

error:
   if (name_list)
      string_list_free(name_list);
   return NULL;
}

/* Creates a list of all possible 'core name' menu
 * strings, for use in 'menu_displaylist' drop-down
 * lists and 'menu_cbs_left/right'
 * > Returns NULL in the event of failure
 * > Returned string list must be free()'d */
struct string_list *manual_content_scan_get_menu_core_name_list(void)
{
   union string_list_elem_attr attr;
   struct string_list *name_list    = string_list_new();
   core_info_list_t *core_info_list = NULL;

   /* Sanity check */
   if (!name_list)
      return NULL;

   attr.i = 0;

   /* Add 'DETECT' entry */
   if (!string_list_append(name_list, msg_hash_to_str(
         MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME_DETECT), attr))
      goto error;

   /* Get core list */
   core_info_get_list(&core_info_list);

   if (core_info_list)
   {
      size_t i;
      core_info_t *core_info = NULL;

      /* Sort cores alphabetically */
      core_info_qsort(core_info_list, CORE_INFO_LIST_SORT_DISPLAY_NAME);

      /* Loop through cores */
      for (i = 0; i < core_info_list->count; i++)
      {
         if ((core_info = core_info_get(core_info_list, i)))
         {
            if (!core_info->display_name || !*core_info->display_name)
               continue;
            /* Add core name to list */
            if (!string_list_append(name_list, core_info->display_name, attr))
               goto error;
         }
      }
   }

   return name_list;

error:
   if (name_list)
      string_list_free(name_list);
   return NULL;
}

/****************/
/* Task Helpers */
/****************/

/* The system name type a scan of the current settings takes: an
 * automatic name is filled when the settings predict it. */
static enum manual_content_scan_system_name_type
      manual_content_scan_task_name_type(void)
{
   if (scan_settings.system_name_type == MANUAL_CONTENT_SCAN_SYSTEM_NAME_AUTO)
   {
      if (scan_settings.db_usage == MANUAL_CONTENT_SCAN_USE_DB_DAT_STRICT ||
          scan_settings.db_usage == MANUAL_CONTENT_SCAN_USE_DB_DAT_LOOSE ||
          scan_settings.db_usage == MANUAL_CONTENT_SCAN_USE_DB_NONE)
         return MANUAL_CONTENT_SCAN_SYSTEM_NAME_CONTENT_DIR;
      if (scan_settings.db_selection == MANUAL_CONTENT_SCAN_SELECT_DB_SPECIFIC)
         return MANUAL_CONTENT_SCAN_SYSTEM_NAME_DATABASE;
   }
   return scan_settings.system_name_type;
}

/* True if a scan of the current settings writes one playlist
 * (target_is_single_determined_playlist). */
bool manual_content_scan_is_single_playlist(void)
{
   return    scan_settings.scan_method != MANUAL_CONTENT_SCAN_METHOD_AUTOMATIC
          && manual_content_scan_task_name_type()
                != MANUAL_CONTENT_SCAN_SYSTEM_NAME_AUTO;
}

/* @name is a whole name: a dot in it is not an extension to replace
 * (Tiger - Game.com, VTech - V.Smile). */
static void manual_content_scan_set_lpl_name(char *s, const char *name,
      size_t len)
{
   size_t _len = strlcpy(s, name, len);
   if (_len < len)
      strlcpy_lit(s + _len, ".lpl", len - _len);
}

/* Parses current manual content scan settings,
 * and extracts all information required to configure
 * a manual content scan task.
 * Returns false if current settings are invalid. */
bool manual_content_scan_get_task_config(
      manual_content_scan_task_config_t *task_config,
      const char *path_dir_playlist)
{
   if (!task_config)
      return false;

   /* Ensure all 'task_config' strings are
    * correctly initialised */
   task_config->playlist_file[0] = '\0';
   task_config->content_dir[0]   = '\0';
   task_config->system_name[0]   = '\0';
   task_config->database_name[0] = '\0';
   task_config->core_name[0]     = '\0';
   task_config->core_path[0]     = '\0';
   task_config->file_exts[0]     = '\0';
   task_config->dat_file_path[0] = '\0';

   /* Ensure defaults for "fully automatic" method. */
   if (scan_settings.scan_method == MANUAL_CONTENT_SCAN_METHOD_AUTOMATIC)
   {
      /* Not all settings are zeroed, but the list below should ensure a consistent task setup */
      scan_settings.system_name_type = MANUAL_CONTENT_SCAN_SYSTEM_NAME_AUTO;
      scan_settings.core_type = MANUAL_CONTENT_SCAN_CORE_DETECT;
      scan_settings.db_usage = MANUAL_CONTENT_SCAN_USE_DB_STRICT;
      scan_settings.db_selection = MANUAL_CONTENT_SCAN_SELECT_DB_AUTO;
      scan_file_exts_custom[0] = '\0';
      scan_settings.search_recursively = true;
      scan_settings.search_archives = true;
   }
   /* Get content directory */
   if (!*scan_content_dir)
      return false;

   task_config->content_is_file = !(manual_content_scan_content_stat(
         scan_content_dir) & RETRO_VFS_STAT_IS_DIRECTORY);

   strlcpy(
         task_config->content_dir,
         scan_content_dir,
         sizeof(task_config->content_dir));

   /* If name is set as automatic, but is predictable, fill it now. */
   scan_settings.system_name_type = manual_content_scan_task_name_type();
   /* Get target playlist ("system name")
    *
    * The system name is not a leaf value: the scan pipeline appends a
    * file extension to it repeatedly, each time into a buffer that is
    * also NAME_MAX_LENGTH bytes.  system_name -> database_name gains
    * ".lpl" (below), database_name -> rdb_name gains ".rdb"
    * (task_database.c), and the basename of that is then copied back
    * into a NAME_MAX_LENGTH buffer by
    * database_info_list_iterate_found_match().
    *
    * Copying up to sizeof(system_name) - 1 here therefore yields a name
    * that provably cannot survive the round trip: at 255 characters the
    * ".lpl" is silently dropped, ".rdb" pushes the string to 259, and
    * the final copy overflows its 256-byte destination.  Reserve room
    * for the longest extension so every hop stays representable. */
   switch (scan_settings.system_name_type)
   {
      case MANUAL_CONTENT_SCAN_SYSTEM_NAME_CONTENT_DIR:
         if (!*scan_system_name_content_dir)
            return false;
         strlcpy(
               task_config->system_name,
               scan_system_name_content_dir,
               sizeof(task_config->system_name)
                  - MANUAL_CONTENT_SCAN_SYSTEM_NAME_EXT_RESERVE);
         task_config->target_is_single_determined_playlist = true;
         break;
      case MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM:
         if (!*scan_system_name_custom)
            return false;
         strlcpy(
               task_config->system_name,
               scan_system_name_custom,
               sizeof(task_config->system_name)
                  - MANUAL_CONTENT_SCAN_SYSTEM_NAME_EXT_RESERVE);
         task_config->target_is_single_determined_playlist = true;
         break;
      case MANUAL_CONTENT_SCAN_SYSTEM_NAME_DATABASE:
         if (!*scan_system_name_database)
            return false;
         strlcpy(
               task_config->system_name,
               scan_system_name_database,
               sizeof(task_config->system_name)
                  - MANUAL_CONTENT_SCAN_SYSTEM_NAME_EXT_RESERVE);
         task_config->target_is_single_determined_playlist = true;
         break;
      case MANUAL_CONTENT_SCAN_SYSTEM_NAME_AUTO:
         /* Leave it as empty intentionally. */
         task_config->target_is_single_determined_playlist = false;
         break;
      default:
         return false;
   }

   /* Store database name and playlist path, depending on scan settings */
   /* Old manual scan method: db name == target playlist */

   manual_content_scan_set_lpl_name(
         task_config->database_name,
         task_config->system_name,
         sizeof(task_config->database_name));

   /* ...which can in turn be used to generate the
    * playlist path */
   if (!path_dir_playlist || !*path_dir_playlist)
      return false;

   fill_pathname_join_special(
         task_config->playlist_file,
         path_dir_playlist,
         task_config->database_name,
         sizeof(task_config->playlist_file));

   if (!*task_config->playlist_file)
      return false;

   if (scan_settings.db_usage == MANUAL_CONTENT_SCAN_USE_DB_STRICT ||
       scan_settings.db_usage == MANUAL_CONTENT_SCAN_USE_DB_LOOSE)
   {

      /* Pass the selected DB name to the task if there is one */
      if (scan_settings.db_selection == MANUAL_CONTENT_SCAN_SELECT_DB_SPECIFIC)
      {
         manual_content_scan_set_lpl_name(
               task_config->database_name,
               scan_system_name_database,
               sizeof(task_config->database_name));
      }
      /* Or make it empty (auto DB select case). The omit_db_ref is handled elsewhere. */
      else
      {
         task_config->database_name[0] = '\0';
         if (scan_settings.system_name_type == MANUAL_CONTENT_SCAN_SYSTEM_NAME_AUTO)
            task_config->playlist_file[0] = '\0';
         /* Abuse dat_file_path for a convenient placeholder value */
         manual_content_scan_set_lpl_name(
               task_config->dat_file_path,
               scan_system_name_content_dir,
               sizeof(task_config->dat_file_path));
      }
   }

   /* Get core name and path */
   switch (scan_settings.core_type)
   {
      case MANUAL_CONTENT_SCAN_CORE_DETECT:
         task_config->core_set = false;
         break;
      case MANUAL_CONTENT_SCAN_CORE_SET:
         task_config->core_set = true;

         if (!*scan_core_name)
            return false;
         if (!*scan_core_path)
            return false;

         strlcpy(
               task_config->core_name,
               scan_core_name,
               sizeof(task_config->core_name));

         strlcpy(
               task_config->core_path,
               scan_core_path,
               sizeof(task_config->core_path));

         break;
      default:
         return false;
   }

   /* Get file extensions list */
   /* Possible improvement: if DB is set (but not core), some filtering may be applied */
   task_config->file_exts_custom_set = false;
   if (*scan_file_exts_custom)
   {
      task_config->file_exts_custom_set = true;
      strlcpy(
            task_config->file_exts,
            scan_file_exts_custom,
            sizeof(task_config->file_exts));
   }
   else if (scan_settings.core_type == MANUAL_CONTENT_SCAN_CORE_SET)
      if (*scan_file_exts_core)
         strlcpy(
               task_config->file_exts,
               scan_file_exts_core,
               sizeof(task_config->file_exts));

   /* Our extension lists are space delimited
    * > dir_list_new() expects vertical bar
    *   delimiters, so find and replace */
   if (*task_config->file_exts)
      string_replace_all_chars(task_config->file_exts, ' ', '|');

   /* Get DAT file path */
   if (  (scan_settings.db_usage == MANUAL_CONTENT_SCAN_USE_DB_DAT_STRICT
       || scan_settings.db_usage == MANUAL_CONTENT_SCAN_USE_DB_DAT_LOOSE)
       && *scan_dat_file_path)
   {
      if (!manual_content_scan_dat_path_is_valid(scan_dat_file_path, NULL))
         return false;
      strlcpy(
            task_config->dat_file_path,
            scan_dat_file_path,
            sizeof(task_config->dat_file_path));
   }

   /* Copy 'search recursively' setting */
   task_config->search_recursively = scan_settings.search_recursively;
   /* Copy 'search inside archives' setting */
   task_config->search_archives    = scan_settings.search_archives;
   /* Set up filter if DB usage requires */
   task_config->filter_dat_content = 
      (scan_settings.db_usage == MANUAL_CONTENT_SCAN_USE_DB_DAT_STRICT) ? true : false;
   /* Copy 'overwrite playlist' setting.
    * A file is only added to its playlist. */
   task_config->overwrite_playlist = scan_settings.overwrite_playlist
         && !task_config->content_is_file;
   /* Copy 'validate_entries' setting */
   task_config->validate_entries   = scan_settings.validate_entries;
   
   task_config->omit_db_reference  = scan_settings.omit_db_ref;
   task_config->db_usage           = scan_settings.db_usage;
   task_config->db_selection       = scan_settings.db_selection;

   /* Captured here, on the main thread, for the worker the scan's
    * handler runs on. The CLI-scan flag is fixed at process start and
    * the serial/CRC toggle is a settings read. */
   task_config->cli_scan_output = retroarch_override_setting_is_set(
         RARCH_OVERRIDE_SETTING_DATABASE_SCAN, NULL);
   task_config->scan_serial_and_crc =
         config_get_ptr()->bools.scan_serial_and_crc;

   return true;
}

/* Creates a list of all valid content in the specified
 * content directory
 * > Returns NULL in the event of failure
 * > Returned string list must be free()'d */
/* Shared policy for both content-list builders below:
 * whether extension filtering applies and whether
 * compressed files must be listed. */
static void manual_content_scan_content_list_policy(
      manual_content_scan_task_config_t *task_config,
      bool *filter_exts, bool *include_compressed)
{
   /* Check whether files should be filtered by
    * extension */
   *filter_exts = *task_config->file_exts;

   /* Check whether compressed files should be
    * included in the directory list
    * > If compressed files are already listed in
    *   the 'file_exts' string, they will be included
    *   automatically
    * > If we don't have a 'file_exts' list, then all
    *   files must be included regardless of type
    * > If user has enabled 'search inside archives',
    *   then compressed files must of course be included */
   *include_compressed = (!*filter_exts || task_config->search_archives);
}

bool manual_content_scan_content_list_iter_new(
      manual_content_scan_task_config_t *task_config,
      struct string_list **list, dir_list_iter_t **iter)
{
   bool filter_exts;
   bool include_compressed;

   if (list)
      *list = NULL;
   if (iter)
      *iter = NULL;

   /* Sanity check */
   if (!task_config || !*task_config->content_dir || !list || !iter)
      return false;

   manual_content_scan_content_list_policy(task_config,
         &filter_exts, &include_compressed);

   if (path_is_directory(task_config->content_dir))
   {
      /* An expected file that has since become a folder */
      if (task_config->content_is_file)
         return false;

      if (!(*list = string_list_new()))
         return false;

      /* Same parameters as the blocking getter's
       * dir_list_new() call: exclude directories and
       * hidden files */
      *iter = dir_list_iter_new(
            task_config->content_dir,
            filter_exts ? task_config->file_exts : NULL,
            false, /* include_dirs */
            false, /* include_hidden */ /* todo: obey global settings? */
            include_compressed,
            task_config->search_recursively,
            *list);

      if (!*iter)
      {
         string_list_free(*list);
         *list = NULL;
         return false;
      }
      return true;
   }

   if (!task_config->content_is_file)
      return false;

   /* A plain-file content dir needs no walk: the list
    * is complete here and *iter stays NULL. */
   if (!(*list = string_list_new()))
      return false;
   {
      union string_list_elem_attr attr;
      attr.i = 0;
      if (!string_list_append(*list, task_config->content_dir, attr))
      {
         string_list_free(*list);
         *list = NULL;
         return false;
      }
   }
   return true;
}

struct string_list *manual_content_scan_get_content_list(
      manual_content_scan_task_config_t *task_config)
{
   bool filter_exts;
   bool include_compressed;
   struct string_list *dir_list = NULL;

   /* Sanity check */
   if (!task_config || !*task_config->content_dir)
      return NULL;

   manual_content_scan_content_list_policy(task_config,
         &filter_exts, &include_compressed);

   if (path_is_directory(task_config->content_dir))
   {
      /* Get directory listing
       * > Exclude directories and hidden files */
      dir_list = dir_list_new(
            task_config->content_dir,
            filter_exts ? task_config->file_exts : NULL,
            false, /* include_dirs */
            false, /* include_hidden */ /* todo: obey global settings? */
            include_compressed,
            task_config->search_recursively
      );
   }
   else
   {
      if ((dir_list = string_list_new()))
      {
         union string_list_elem_attr attr;
         attr.i = 0;
         string_list_append(dir_list, task_config->content_dir, attr);
      }
   }

   /* Sanity check */
   if (!dir_list || dir_list->size < 1)
      goto error;

   /* Ensure list is in alphabetical order
    * > Not strictly required, but task status
    *   messages will be unintuitive if we leave
    *   the order 'random' */
   dir_list_sort(dir_list, true);

   return dir_list;

error:
   if (dir_list)
      string_list_free(dir_list);
   return NULL;
}

/* Converts specified content path string to 'real'
 * file path for use in playlists - i.e. handles
 * identification of content *inside* archive files.
 * Returns false if specified content is invalid. */
bool manual_content_scan_get_playlist_content_path(
      manual_content_scan_task_config_t *task_config,
      const char *content_path, int content_type,
      char *s, size_t len)
{
   /* Sanity check */
   if (!task_config || (!content_path || !*content_path))
      return false;
   /* Allow paths inside archives, but do not add directories in or outside of archives. */
   if (!path_is_valid(content_path))
   {
      if(!(string_index_last_occurance(content_path,'#') > 0) ||
          (string_index_last_occurance(content_path,'/') + 1 == (int)strlen(content_path)))
         return false;
   }
      
   if (path_is_directory(content_path))
      return false;
   /* In all cases, base content path must be
    * copied to @s */
   strlcpy(s, content_path, len);
   return true;
   /* no need to do all the other magic here any more */
}

/* Extracts content 'label' (name) from content path
 * > If a DAT file is specified, performs a lookup
 *   of content file name in an attempt to find a
 *   valid 'description' string.
 * Returns false if specified content is invalid. */
bool manual_content_scan_get_playlist_content_label(
      const char *content_path, logiqx_dat_t *dat_file,
      bool filter_dat_content,
      char *s, size_t len)
{
   /* Sanity check */
   if (!content_path || !*content_path)
      return false;

   /* In most cases, content label is just the
    * filename without extension */
   fill_pathname(s, path_basename(content_path),
         "", len);

   if (!s || !*s)
      return false;

   /* Check if a DAT file has been specified */
   if (dat_file)
   {
      logiqx_dat_game_info_t game_info;

      /* Search for current content
       * > If content is not listed in DAT file,
       *   use existing filename without extension */
      if (logiqx_dat_search(dat_file, s, &game_info))
      {
         /* BIOS files should always be skipped */
         if (game_info.is_bios)
            return false;
         /* Only include 'runnable' content */
         if (!game_info.is_runnable)
            return false;
         /* Copy game description */
         if (*game_info.description)
         {
            strlcpy(s, game_info.description, len);
            return true;
         }
      }

      /* If we are applying a DAT file filter,
       * unlisted content should be skipped */
      if (filter_dat_content)
         return false;
   }
   return true;
}

/* Adds specified content to playlist, if not already
 * present */
void manual_content_scan_add_content_to_playlist(
      manual_content_scan_task_config_t *task_config,
      playlist_t *playlist, const char *content_path,
      int content_type, logiqx_dat_t *dat_file)
{
   char playlist_content_path[PATH_MAX_LENGTH];

   /* Sanity check */
   if (!task_config || !playlist)
      return;

   /* Get 'actual' content path */
   if (!manual_content_scan_get_playlist_content_path(
         task_config, content_path, content_type,
         playlist_content_path, sizeof(playlist_content_path)))
      return;

   /* Check whether content is already included
    * in playlist */
   if (!playlist_entry_exists(playlist, playlist_content_path))
   {
      char label[NAME_MAX_LENGTH];
      struct playlist_entry entry = {0};

      label[0] = '\0';

      /* Get entry label */
      if (!manual_content_scan_get_playlist_content_label(
            playlist_content_path, dat_file,
            task_config->filter_dat_content,
            label, sizeof(label)))
         return;

      /* Configure playlist entry
       * > The push function reads our entry as const,
       *   so these casts are safe */
      entry.path       = (char*)playlist_content_path;
      entry.label      = label;
      entry.core_path  = (char*)FILE_PATH_DETECT;
      entry.core_name  = (char*)FILE_PATH_DETECT;
      entry.crc32      = (char*)"00000000|crc";
      entry.db_name    = task_config->database_name;
      PLAYLIST_SET_ENTRY_SLOT(&entry, 0);

      /* Add entry to playlist */
      playlist_push(playlist, &entry);
   }
}
