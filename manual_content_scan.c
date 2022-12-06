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

#include <file/file_path.h>
#include <file/archive_file.h>
#include <string/stdstring.h>
#include <lists/dir_list.h>
#include <retro_miscellaneous.h>

#include "msg_hash.h"
#include "list_special.h"
#include "core_info.h"
#include "file_path_special.h"

#include "frontend/frontend_driver.h"

#include "manual_content_scan.h"

/* Holds all configuration parameters associated
 * with a manual content scan */
typedef struct
{
   enum manual_content_scan_system_name_type system_name_type;
   enum manual_content_scan_core_type core_type;

   char content_dir[PATH_MAX_LENGTH];
   char system_name_content_dir[PATH_MAX_LENGTH];
   char system_name_database[PATH_MAX_LENGTH];
   char system_name_custom[PATH_MAX_LENGTH];
   char core_name[PATH_MAX_LENGTH];
   char core_path[PATH_MAX_LENGTH];
   char file_exts_core[PATH_MAX_LENGTH];
   char file_exts_custom[PATH_MAX_LENGTH];
   char dat_file_path[PATH_MAX_LENGTH];

   bool search_recursively;
   bool search_archives;
   bool filter_dat_content;
   bool overwrite_playlist;
   bool validate_entries;
} scan_settings_t;

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
static scan_settings_t scan_settings = {
   MANUAL_CONTENT_SCAN_SYSTEM_NAME_CONTENT_DIR, /* system_name_type */
   MANUAL_CONTENT_SCAN_CORE_DETECT,             /* core_type */
   "",                                          /* content_dir */
   "",                                          /* system_name_content_dir */
   "",                                          /* system_name_database */
   "",                                          /* system_name_custom */
   "",                                          /* core_name */
   "",                                          /* core_path */
   "",                                          /* file_exts_core */
   "",                                          /* file_exts_custom */
   "",                                          /* dat_file_path */
   true,                                        /* search_recursively */
   false,                                       /* search_archives */
   false,                                       /* filter_dat_content */
   false,                                       /* overwrite_playlist */
   false                                        /* validate_entries */
};

/*****************/
/* Configuration */
/*****************/

/* Pointer access */

/* Returns a pointer to the internal
 * 'content_dir' string */
char *manual_content_scan_get_content_dir_ptr(void)
{
   return scan_settings.content_dir;
}

/* Returns a pointer to the internal
 * 'system_name_custom' string */
char *manual_content_scan_get_system_name_custom_ptr(void)
{
   return scan_settings.system_name_custom;
}

/* Returns size of the internal
 * 'system_name_custom' string */
size_t manual_content_scan_get_system_name_custom_size(void)
{
   return sizeof(scan_settings.system_name_custom);
}

/* Returns a pointer to the internal
 * 'file_exts_custom' string */
char *manual_content_scan_get_file_exts_custom_ptr(void)
{
   return scan_settings.file_exts_custom;
}

/* Returns size of the internal
 * 'file_exts_custom' string */
size_t manual_content_scan_get_file_exts_custom_size(void)
{
   return sizeof(scan_settings.file_exts_custom);
}

/* Returns a pointer to the internal
 * 'dat_file_path' string */
char *manual_content_scan_get_dat_file_path_ptr(void)
{
   return scan_settings.dat_file_path;
}

/* Returns size of the internal
 * 'dat_file_path' string */
size_t manual_content_scan_get_dat_file_path_size(void)
{
   return sizeof(scan_settings.dat_file_path);
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
   if (string_is_empty(file_exts))
      return;

   string_remove_all_chars(file_exts, '.');
   string_to_lower(file_exts);
   string_trim_whitespace(file_exts);
}

/* Removes invalid characters from
 * 'system_name_custom' string */
void manual_content_scan_scrub_system_name_custom(void)
{
   char *scrub_char_pointer = NULL;

   if (string_is_empty(scan_settings.system_name_custom))
      return;

   /* Scrub characters that are not cross-platform
    * and/or violate the No-Intro filename standard:
    * http://datomatic.no-intro.org/stuff/The%20Official%20No-Intro%20Convention%20(20071030).zip
    * Replace these characters with underscores */
   while ((scrub_char_pointer = 
            strpbrk(scan_settings.system_name_custom, "&*/:`\"<>?\\|")))
      *scrub_char_pointer = '_';
}

/* Removes period (full stop) characters from
 * 'file_exts_custom' string and converts to
 * lower case */
void manual_content_scan_scrub_file_exts_custom(void)
{
   manual_content_scan_scrub_file_exts(scan_settings.file_exts_custom);
}

/* Checks 'dat_file_path' string and resets it
 * if invalid */
enum manual_content_scan_dat_file_path_status
      manual_content_scan_validate_dat_file_path(void)
{
   enum manual_content_scan_dat_file_path_status dat_file_path_status =
         MANUAL_CONTENT_SCAN_DAT_FILE_UNSET;

   /* Check if 'dat_file_path' has been set */
   if (!string_is_empty(scan_settings.dat_file_path))
   {
      uint64_t file_size;

      /* Check if path itself is valid */
      if (logiqx_dat_path_is_valid(scan_settings.dat_file_path, &file_size))
      {
         uint64_t free_memory = frontend_driver_get_free_memory();
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
      scan_settings.dat_file_path[0] = '\0';

   return dat_file_path_status;
}

/* Menu setters */

/* Sets content directory for next manual scan
 * operation.
 * Returns true if content directory is valid. */
bool manual_content_scan_set_menu_content_dir(const char *content_dir)
{
   size_t len;
   const char *dir_name = NULL;

   /* Sanity check */
   if (string_is_empty(content_dir))
      goto error;

   if (!path_is_directory(content_dir))
      goto error;

   /* Copy directory path to settings struct */
   strlcpy(
         scan_settings.content_dir,
         content_dir,
         sizeof(scan_settings.content_dir));

   /* Remove trailing slash, if required */
   len = strlen(scan_settings.content_dir);
   if (len <= 0)
      goto error;

   if (scan_settings.content_dir[len - 1] == PATH_DEFAULT_SLASH_C())
      scan_settings.content_dir[len - 1] = '\0';

   /* Handle case where path was a single slash... */
   if (string_is_empty(scan_settings.content_dir))
      goto error;

   /* Get directory name (used as system name
    * when scan_settings.system_name_type ==
    * MANUAL_CONTENT_SCAN_SYSTEM_NAME_CONTENT_DIR) */
   dir_name = path_basename(scan_settings.content_dir);

   if (string_is_empty(dir_name))
      goto error;

   /* Copy directory name to settings struct */
   strlcpy(
         scan_settings.system_name_content_dir,
         dir_name,
         sizeof(scan_settings.system_name_content_dir));

   return true;

error:
   /* Directory is invalid - reset internal
    * content directory and associated 'directory'
    * system name */
   scan_settings.content_dir[0]             = '\0';
   scan_settings.system_name_content_dir[0] = '\0';
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
       (scan_settings.system_name_type == MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM))
      scan_settings.system_name_database[0] = '\0';
   else
   {
      /* We are using a database name... */
      if (string_is_empty(system_name))
         goto error;

      /* Copy database name to settings struct */
      strlcpy(
            scan_settings.system_name_database,
            system_name,
            sizeof(scan_settings.system_name_database));
   }

   return true;

error:
   /* Input parameters are invalid - reset internal
    * 'system_name_type' and 'system_name_database' */
   scan_settings.system_name_type        = MANUAL_CONTENT_SCAN_SYSTEM_NAME_CONTENT_DIR;
   scan_settings.system_name_database[0] = '\0';
   return false;
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
      scan_settings.core_name[0]      = '\0';
      scan_settings.core_path[0]      = '\0';
      scan_settings.file_exts_core[0] = '\0';
   }
   else
   {
      core_info_list_t *core_info_list = NULL;
      core_info_t *core_info           = NULL;
      bool core_found                  = false;
      size_t i;

      /* We are using a manually set core... */
      if (string_is_empty(core_name))
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

               /* Copy core path to settings struct */
               if (string_is_empty(core_info->path))
                  goto error;

               strlcpy(
                     scan_settings.core_path,
                     core_info->path,
                     sizeof(scan_settings.core_path));

               /* Copy core name to settings struct */
               strlcpy(
                     scan_settings.core_name,
                     core_info->display_name,
                     sizeof(scan_settings.core_name));

               /* Copy supported extensions to settings
                * struct, if required */
               if (!string_is_empty(core_info->supported_extensions))
               {
                  strlcpy(
                        scan_settings.file_exts_core,
                        core_info->supported_extensions,
                        sizeof(scan_settings.file_exts_core));

                  /* Core info extensions are delimited by
                   * vertical bars. For internal consistency,
                   * replace them with spaces */
                  string_replace_all_chars(scan_settings.file_exts_core, '|', ' ');

                  /* Apply standard scrubbing/clean-up
                   * (should not be required, but must handle the
                   * case where a core info file is incorrectly
                   * formatted) */
                  manual_content_scan_scrub_file_exts(scan_settings.file_exts_core);
               }
               else
                  scan_settings.file_exts_core[0] = '\0';

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
   scan_settings.core_name[0]      = '\0';
   scan_settings.core_path[0]      = '\0';
   scan_settings.file_exts_core[0] = '\0';
   return false;
}

/* Sets all parameters for the next manual scan
 * operation according the to recorded values in
 * the specified playlist.
 * Returns MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_OK
 * if playlist contains a valid scan record. */
enum manual_content_scan_playlist_refresh_status
      manual_content_scan_set_menu_from_playlist(playlist_t *playlist,
            const char *path_content_database, bool show_hidden_files)
{
   const char *playlist_path    = NULL;
   const char *playlist_file    = NULL;
   const char *content_dir      = NULL;
   const char *core_name        = NULL;
   const char *file_exts        = NULL;
   const char *dat_file_path    = NULL;
   bool search_recursively      = false;
   bool search_archives         = false;
   bool filter_dat_content      = false;
#ifdef HAVE_LIBRETRODB
   struct string_list *rdb_list = NULL;
#endif
   enum manual_content_scan_system_name_type
         system_name_type       = MANUAL_CONTENT_SCAN_SYSTEM_NAME_CONTENT_DIR;
   enum manual_content_scan_core_type
         core_type              = MANUAL_CONTENT_SCAN_CORE_DETECT;
   enum manual_content_scan_playlist_refresh_status
         playlist_status        = MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_OK;
   char system_name[PATH_MAX_LENGTH];

   system_name[0] = '\0';

   if (!playlist_scan_refresh_enabled(playlist))
   {
      playlist_status = MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_MISSING_CONFIG;
      goto end;
   }

   /* Read scan parameters from playlist */
   playlist_path      = playlist_get_conf_path(playlist);
   content_dir        = playlist_get_scan_content_dir(playlist);
   core_name          = playlist_get_default_core_name(playlist);
   file_exts          = playlist_get_scan_file_exts(playlist);
   dat_file_path      = playlist_get_scan_dat_file_path(playlist);

   search_recursively = playlist_get_scan_search_recursively(playlist);
   search_archives    = playlist_get_scan_search_archives(playlist);
   filter_dat_content = playlist_get_scan_filter_dat_content(playlist);

   /* Determine system name (playlist basename
    * without extension) */
   if (string_is_empty(playlist_path))
   {
      /* Cannot happen, but would constitute a
       * 'system name' error */
      playlist_status = MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_INVALID_SYSTEM_NAME;
      goto end;
   }

   if ((playlist_file = path_basename(playlist_path)))
   {
      strlcpy(system_name, playlist_file, sizeof(system_name));
      path_remove_extension(system_name);
   }

   if (string_is_empty(system_name))
   {
      /* Cannot happen, but would constitute a
       * 'system name' error */
      playlist_status = MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_INVALID_SYSTEM_NAME;
      goto end;
   }

   /* Set content directory */
   if (!manual_content_scan_set_menu_content_dir(content_dir))
   {
      playlist_status = MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_INVALID_CONTENT_DIR;
      goto end;
   }

   /* Set system name */
#ifdef HAVE_LIBRETRODB
   /* > If platform has database support, get names
    *   of all installed database files */
   rdb_list = dir_list_new_special(
         path_content_database,
         DIR_LIST_DATABASES, NULL, show_hidden_files);

   if (rdb_list && rdb_list->size)
   {
      size_t i;

      /* Loop over database files */
      for (i = 0; i < rdb_list->size; i++)
      {
         const char *rdb_path = rdb_list->elems[i].data;
         const char *rdb_file = NULL;
         char rdb_name[PATH_MAX_LENGTH];

         rdb_name[0] = '\0';

         /* Sanity check */
         if (string_is_empty(rdb_path))
            continue;

         rdb_file = path_basename(rdb_path);

         if (string_is_empty(rdb_file))
            continue;

         /* Remove file extension */
         strlcpy(rdb_name, rdb_file, sizeof(rdb_name));
         path_remove_extension(rdb_name);

         if (string_is_empty(rdb_name))
            continue;

         /* Check whether playlist system name
          * matches current database file */
         if (string_is_equal(system_name, rdb_name))
         {
            system_name_type = MANUAL_CONTENT_SCAN_SYSTEM_NAME_DATABASE;
            break;
         }
      }
   }

   string_list_free(rdb_list);
#endif

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
            scan_settings.system_name_content_dir))
         scan_settings.system_name_custom[0] = '\0';
      else
      {
         /* Playlist is using a custom system name */
         system_name_type = MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM;
         strlcpy(scan_settings.system_name_custom, system_name,
               sizeof(scan_settings.system_name_custom));
      }
   }

   if (!manual_content_scan_set_menu_system_name(
         system_name_type, system_name))
   {
      playlist_status = MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_INVALID_SYSTEM_NAME;
      goto end;
   }

   /* Set core path/name */
   if (!string_is_empty(core_name) &&
       !string_is_equal(core_name, FILE_PATH_DETECT))
      core_type = MANUAL_CONTENT_SCAN_CORE_SET;

   if (!manual_content_scan_set_menu_core_name(
         core_type, core_name))
   {
      playlist_status = MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_INVALID_CORE;
      goto end;
   }

   /* Set custom file extensions */
   if (string_is_empty(file_exts))
      scan_settings.file_exts_custom[0] = '\0';
   else
   {
      strlcpy(scan_settings.file_exts_custom, file_exts,
            sizeof(scan_settings.file_exts_custom));

      /* File extensions read from playlist should
       * be correctly formatted, with '|' characters
       * as delimiters
       * > For menu purposes, must replace these
       *   delimiters with space characters
       * > Additionally scrub the resultant string,
       *   to handle the case where a user has
       *   'corrupted' it by manually tampering with
       *   the playlist file */
      string_replace_all_chars(scan_settings.file_exts_custom, '|', ' ');
      manual_content_scan_scrub_file_exts(scan_settings.file_exts_custom);
   }

   /* Set DAT file path */
   if (string_is_empty(dat_file_path))
      scan_settings.dat_file_path[0] = '\0';
   else
   {
      strlcpy(scan_settings.dat_file_path, dat_file_path,
            sizeof(scan_settings.dat_file_path));

      switch (manual_content_scan_validate_dat_file_path())
      {
         case MANUAL_CONTENT_SCAN_DAT_FILE_INVALID:
            playlist_status = MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_INVALID_DAT_FILE;
            goto end;
         case MANUAL_CONTENT_SCAN_DAT_FILE_TOO_LARGE:
            playlist_status = MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_DAT_FILE_TOO_LARGE;
            goto end;
         default:
            /* No action required */
            break;
      }
   }

   /* Set remaining boolean parameters */
   scan_settings.search_recursively = search_recursively;
   scan_settings.search_archives    = search_archives;
   scan_settings.filter_dat_content = filter_dat_content;
   /* When refreshing a playlist:
    * > We never overwrite the existing file
    * > We always validate entries in the
    *   existing file */
   scan_settings.overwrite_playlist = false;
   scan_settings.validate_entries   = true;

end:
   return playlist_status;
}

/* Menu getters */

/* Fetches content directory for next manual scan
 * operation.
 * Returns true if content directory is valid. */
bool manual_content_scan_get_menu_content_dir(const char **content_dir)
{
   if (!content_dir)
      return false;

   if (string_is_empty(scan_settings.content_dir))
      return false;

   *content_dir = scan_settings.content_dir;

   return true;
}

/* Fetches system name for the next manual scan operation.
 * Returns true if system name is valid.
 * NOTE: This corresponds to the 'System Name' value
 * displayed in menus - this is not identical to the
 * actual system name used when generating the playlist */
bool manual_content_scan_get_menu_system_name(const char **system_name)
{
   if (!system_name)
      return false;

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
      case MANUAL_CONTENT_SCAN_SYSTEM_NAME_DATABASE:
         if (string_is_empty(scan_settings.system_name_database))
            return false;
         else
         {
            *system_name = scan_settings.system_name_database;
            return true;
         }
      default:
         break;
   }

   return false;
}

/* Fetches core name for the next manual scan operation.
 * Returns true if core name is valid. */
bool manual_content_scan_get_menu_core_name(const char **core_name)
{
   if (!core_name)
      return false;

   switch (scan_settings.core_type)
   {
      case MANUAL_CONTENT_SCAN_CORE_DETECT:
         *core_name = msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME_DETECT);
         return true;
      case MANUAL_CONTENT_SCAN_CORE_SET:
         if (string_is_empty(scan_settings.core_name))
            return false;
         else
         {
            *core_name = scan_settings.core_name;
            return true;
         }
      default:
         break;
   }

   return false;
}

/* Menu utility functions */

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
            const char *rdb_path = rdb_list->elems[i].data;
            const char *rdb_file = NULL;
            char rdb_name[PATH_MAX_LENGTH];

            rdb_name[0] = '\0';

            /* Sanity check */
            if (string_is_empty(rdb_path))
               continue;

            rdb_file = path_basename(rdb_path);

            if (string_is_empty(rdb_file))
               continue;

            /* Remove file extension */
            strlcpy(rdb_name, rdb_file, sizeof(rdb_name));
            path_remove_extension(rdb_name);

            if (string_is_empty(rdb_name))
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
   struct string_list *name_list    = string_list_new();
   core_info_list_t *core_info_list = NULL;
   union string_list_elem_attr attr;

   /* Sanity check */
   if (!name_list)
      goto error;

   attr.i = 0;

   /* Add 'DETECT' entry */
   if (!string_list_append(name_list, msg_hash_to_str(
         MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME_DETECT), attr))
      goto error;

   /* Get core list */
   core_info_get_list(&core_info_list);

   if (core_info_list)
   {
      core_info_t *core_info = NULL;
      size_t i;

      /* Sort cores alphabetically */
      core_info_qsort(core_info_list, CORE_INFO_LIST_SORT_DISPLAY_NAME);

      /* Loop through cores */
      for (i = 0; i < core_info_list->count; i++)
      {
         core_info = NULL;
         core_info = core_info_get(core_info_list, i);

         if (core_info)
         {
            if (string_is_empty(core_info->display_name))
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

/* Parses current manual content scan settings,
 * and extracts all information required to configure
 * a manual content scan task.
 * Returns false if current settings are invalid. */
bool manual_content_scan_get_task_config(
      manual_content_scan_task_config_t *task_config,
      const char *path_dir_playlist
      )
{
   size_t len;
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

   /* Get content directory */
   if (string_is_empty(scan_settings.content_dir))
      return false;

   if (!path_is_directory(scan_settings.content_dir))
      return false;

   strlcpy(
         task_config->content_dir,
         scan_settings.content_dir,
         sizeof(task_config->content_dir));

   /* Get system name */
   switch (scan_settings.system_name_type)
   {
      case MANUAL_CONTENT_SCAN_SYSTEM_NAME_CONTENT_DIR:
         if (string_is_empty(scan_settings.system_name_content_dir))
            return false;

         strlcpy(
               task_config->system_name,
               scan_settings.system_name_content_dir,
               sizeof(task_config->system_name));

         break;
      case MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM:
         if (string_is_empty(scan_settings.system_name_custom))
            return false;

         strlcpy(
               task_config->system_name,
               scan_settings.system_name_custom,
               sizeof(task_config->system_name));

         break;
      case MANUAL_CONTENT_SCAN_SYSTEM_NAME_DATABASE:
         if (string_is_empty(scan_settings.system_name_database))
            return false;

         strlcpy(
               task_config->system_name,
               scan_settings.system_name_database,
               sizeof(task_config->system_name));

         break;
      default:
         return false;
   }

   /* Now we have a valid system name, can generate
    * a 'database' name... */
   len = strlcpy(
         task_config->database_name,
         task_config->system_name,
         sizeof(task_config->database_name));
   task_config->database_name[len  ] = '.';
   task_config->database_name[len+1] = 'l';
   task_config->database_name[len+2] = 'p';
   task_config->database_name[len+3] = 'l';
   task_config->database_name[len+4] = '\0';

   /* ...which can in turn be used to generate the
    * playlist path */
   if (string_is_empty(path_dir_playlist))
      return false;

   fill_pathname_join_special(
         task_config->playlist_file,
         path_dir_playlist,
         task_config->database_name,
         sizeof(task_config->playlist_file));

   if (string_is_empty(task_config->playlist_file))
      return false;

   /* Get core name and path */
   switch (scan_settings.core_type)
   {
      case MANUAL_CONTENT_SCAN_CORE_DETECT:
         task_config->core_set = false;
         break;
      case MANUAL_CONTENT_SCAN_CORE_SET:
         task_config->core_set = true;

         if (string_is_empty(scan_settings.core_name))
            return false;
         if (string_is_empty(scan_settings.core_path))
            return false;

         strlcpy(
               task_config->core_name,
               scan_settings.core_name,
               sizeof(task_config->core_name));

         strlcpy(
               task_config->core_path,
               scan_settings.core_path,
               sizeof(task_config->core_path));

         break;
      default:
         return false;
   }

   /* Get file extensions list */
   task_config->file_exts_custom_set = false;
   if (!string_is_empty(scan_settings.file_exts_custom))
   {
      task_config->file_exts_custom_set = true;
      strlcpy(
            task_config->file_exts,
            scan_settings.file_exts_custom,
            sizeof(task_config->file_exts));
   }
   else if (scan_settings.core_type == MANUAL_CONTENT_SCAN_CORE_SET)
      if (!string_is_empty(scan_settings.file_exts_core))
         strlcpy(
               task_config->file_exts,
               scan_settings.file_exts_core,
               sizeof(task_config->file_exts));

   /* Our extension lists are space delimited
    * > dir_list_new() expects vertical bar
    *   delimiters, so find and replace */
   if (!string_is_empty(task_config->file_exts))
      string_replace_all_chars(task_config->file_exts, ' ', '|');

   /* Get DAT file path */
   if (!string_is_empty(scan_settings.dat_file_path))
   {
      if (!logiqx_dat_path_is_valid(scan_settings.dat_file_path, NULL))
         return false;

      strlcpy(
            task_config->dat_file_path,
            scan_settings.dat_file_path,
            sizeof(task_config->dat_file_path));
   }

   /* Copy 'search recursively' setting */
   task_config->search_recursively = scan_settings.search_recursively;
   /* Copy 'search inside archives' setting */
   task_config->search_archives    = scan_settings.search_archives;
   /* Copy 'DAT file filter' setting */
   task_config->filter_dat_content = scan_settings.filter_dat_content;
   /* Copy 'overwrite playlist' setting */
   task_config->overwrite_playlist = scan_settings.overwrite_playlist;
   /* Copy 'validate_entries' setting */
   task_config->validate_entries   = scan_settings.validate_entries;

   return true;
}

/* Creates a list of all valid content in the specified
 * content directory
 * > Returns NULL in the event of failure
 * > Returned string list must be free()'d */
struct string_list *manual_content_scan_get_content_list(
      manual_content_scan_task_config_t *task_config)
{
   struct string_list *dir_list = NULL;
   bool filter_exts;
   bool include_compressed;

   /* Sanity check */
   if (!task_config)
      goto error;

   if (string_is_empty(task_config->content_dir))
      goto error;

   /* Check whether files should be filtered by
    * extension */
   filter_exts = !string_is_empty(task_config->file_exts);

   /* Check whether compressed files should be
    * included in the directory list
    * > If compressed files are already listed in
    *   the 'file_exts' string, they will be included
    *   automatically
    * > If we don't have a 'file_exts' list, then all
    *   files must be included regardless of type
    * > If user has enabled 'search inside archives',
    *   then compressed files must of course be included */
   include_compressed = (!filter_exts || task_config->search_archives);

   /* Get directory listing
    * > Exclude directories and hidden files */
   dir_list = dir_list_new(
         task_config->content_dir,
         filter_exts ? task_config->file_exts : NULL,
         false, /* include_dirs */
         false, /* include_hidden */
         include_compressed,
         task_config->search_recursively
   );

   /* Sanity check */
   if (!dir_list)
      goto error;

   if (dir_list->size < 1)
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
static bool manual_content_scan_get_playlist_content_path(
      manual_content_scan_task_config_t *task_config,
      const char *content_path, int content_type,
      char *s, size_t len)
{
   size_t _len;
   struct string_list *archive_list = NULL;

   /* Sanity check */
   if (!task_config || string_is_empty(content_path))
      return false;

   if (!path_is_valid(content_path))
      return false;

   /* In all cases, base content path must be
    * copied to @s */
   _len = strlcpy(s, content_path, len);

   /* Check whether this is an archive file
    * requiring special attention... */
   if ((content_type == RARCH_COMPRESSED_ARCHIVE) &&
       task_config->search_archives)
   {
      bool filter_exts         = !string_is_empty(task_config->file_exts);
      const char *archive_file = NULL;

      /* Important note:
       * > If an archive file of a particular type is
       *   included in the task_config->file_exts list,
       *   dir_list_new() will assign it a file type of
       *   RARCH_PLAIN_FILE
       * > Thus we will only reach this point if
       *   (a) We are not filtering by extension
       *   (b) This is an archive file type *not*
       *       already included in the supported
       *       extensions list
       * > These guarantees substantially reduce the
       *   complexity of the following code... */

      /* Get archive file contents */
      if (!(archive_list = file_archive_get_file_list(
            content_path, filter_exts ? task_config->file_exts : NULL)))
         goto error;

      if (archive_list->size < 1)
         goto error;

      /* Get first file contained in archive */
      dir_list_sort(archive_list, true);
      archive_file = archive_list->elems[0].data;
      if (string_is_empty(archive_file))
         goto error;

      /* Have to take care to ensure that we don't make
       * a mess of arcade content...
       * > If we are filtering by extension, then the
       *   archive file itself is *not* valid content,
       *   so link to the first file inside the archive
       * > If we are not filtering by extension, then:
       *   - If archive contains one valid file, assume
       *     it is a compressed ROM
       *   - If archive contains multiple files, have to
       *     assume it is MAME/FBA-style content, where
       *     only the archive itself is valid */
      if (filter_exts || (archive_list->size == 1))
      {
         /* Build path to file inside archive */
         s[_len  ] = '#';
         s[_len+1] = '\0';
         strlcat(s, archive_file, len);
      }

      string_list_free(archive_list);
   }

   return true;

error:
   if (archive_list)
      string_list_free(archive_list);
   return false;
}

/* Extracts content 'label' (name) from content path
 * > If a DAT file is specified, performs a lookup
 *   of content file name in an attempt to find a
 *   valid 'description' string.
 * Returns false if specified content is invalid. */
static bool manual_content_scan_get_playlist_content_label(
      const char *content_path, logiqx_dat_t *dat_file,
      bool filter_dat_content,
      char *content_label, size_t len)
{
   /* Sanity check */
   if (string_is_empty(content_path))
      return false;

   /* In most cases, content label is just the
    * filename without extension */
   fill_pathname(content_label, path_basename(content_path), 
         "", len);

   if (string_is_empty(content_label))
      return false;

   /* Check if a DAT file has been specified */
   if (dat_file)
   {
      bool content_found = false;
      logiqx_dat_game_info_t game_info;

      /* Search for current content
       * > If content is not listed in DAT file,
       *   use existing filename without extension */
      if (logiqx_dat_search(dat_file, content_label, &game_info))
      {
         /* BIOS files should always be skipped */
         if (game_info.is_bios)
            return false;

         /* Only include 'runnable' content */
         if (!game_info.is_runnable)
            return false;

         /* Copy game description */
         if (!string_is_empty(game_info.description))
         {
            strlcpy(content_label, game_info.description, len);
            content_found = true;
         }
      }

      /* If we are applying a DAT file filter,
       * unlisted content should be skipped */
      if (!content_found && filter_dat_content)
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
      struct playlist_entry entry = {0};
      char label[PATH_MAX_LENGTH];

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
      entry.entry_slot = 0;

      /* Add entry to playlist */
      playlist_push(playlist, &entry);
   }
}
