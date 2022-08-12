/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2019 - Brad Parker
 *  Copyright (C) 2019-2020 - James Leaver
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

#include <string/stdstring.h>
#include <lists/string_list.h>
#include <file/file_path.h>
#include <streams/interface_stream.h>
#include <streams/file_stream.h>
#include <lists/dir_list.h>
#include <time/rtime.h>
#include <retro_miscellaneous.h>

#include "frontend/frontend_driver.h"
#include "file_path_special.h"
#include "verbosity.h"

#include "core_backup.h"

/* Holds all entries in a core backup list */
struct core_backup_list
{
   core_backup_list_entry_t *entries;
   size_t size;
   size_t capacity;
};

/*********************/
/* Utility Functions */
/*********************/

/* Generates backup directory path for specified core.
 * Returns false if 'core_path' and/or 'dir_core_assets'
 * are invalid, or a filesystem error occurs */
static bool core_backup_get_backup_dir(
      const char *dir_libretro, const char *dir_core_assets,
      const char *core_filename,
      char *backup_dir, size_t len)
{
   char *last_underscore = NULL;
   char core_file_id[PATH_MAX_LENGTH];
   char tmp[PATH_MAX_LENGTH];

   /* Extract core file 'ID' (name without extension + suffix)
    * from core path */
   if (string_is_empty(dir_libretro) ||
       string_is_empty(core_filename) ||
       (len < 1))
      return false;

   strlcpy(core_file_id, core_filename, sizeof(core_file_id));

   /* > Remove file extension */
   path_remove_extension(core_file_id);

   if (string_is_empty(core_file_id))
      return false;

   /* > Remove platform-specific file name suffix,
    *   if required */
   last_underscore = strrchr(core_file_id, '_');

   if (!string_is_empty(last_underscore))
      if (!string_is_equal(last_underscore, "_libretro"))
         *last_underscore = '\0';

   if (string_is_empty(core_file_id))
      return false;

   /* Get core backup directory
    * > If no assets directory is defined, use
    *   core directory as a base */
   fill_pathname_join_special(tmp,
         string_is_empty(dir_core_assets)
         ? dir_libretro 
         : dir_core_assets,
               "core_backups", sizeof(tmp));

   fill_pathname_join_special(backup_dir, tmp,
         core_file_id, len);

   if (string_is_empty(backup_dir))
      return false;

   /* > Create directory, if required */
   if (!path_is_directory(backup_dir))
   {
      if (!path_mkdir(backup_dir))
      {
         RARCH_ERR("[core backup] Failed to create backup directory: %s.\n", backup_dir);
         return false;
      }
   }

   return true;
}

/* Generates a timestamped core backup file path from
 * the specified core path. Returns true if successful */
bool core_backup_get_backup_path(
      const char *core_path, uint32_t crc,
      enum core_backup_mode backup_mode,
      const char *dir_core_assets,
      char *backup_path, size_t len)
{
   time_t current_time;
   struct tm time_info;
   const char *core_filename = NULL;
   char core_dir[PATH_MAX_LENGTH];
   char backup_dir[PATH_MAX_LENGTH];
   char backup_filename[PATH_MAX_LENGTH];

   backup_dir[0]      = '\0';
   backup_filename[0] = '\0';

   /* Get core filename and parent directory */
   if (string_is_empty(core_path))
      return false;

   core_filename = path_basename(core_path);

   if (string_is_empty(core_filename))
      return false;

   fill_pathname_parent_dir(core_dir, core_path, sizeof(core_dir));

   if (string_is_empty(core_dir))
      return false;

   /* Get backup directory */
   if (!core_backup_get_backup_dir(core_dir, dir_core_assets, core_filename,
         backup_dir, sizeof(backup_dir)))
      return false;

   /* Get current time */
   time(&current_time);
   rtime_localtime(&current_time, &time_info);

   /* Generate backup filename */
   snprintf(backup_filename, sizeof(backup_filename),
         "%s.%04u%02u%02uT%02u%02u%02u.%08lx.%u%s",
         core_filename,
         (unsigned)time_info.tm_year + 1900,
         (unsigned)time_info.tm_mon + 1,
         (unsigned)time_info.tm_mday,
         (unsigned)time_info.tm_hour,
         (unsigned)time_info.tm_min,
         (unsigned)time_info.tm_sec,
         (unsigned long)crc,
         (unsigned)backup_mode,
         FILE_PATH_CORE_BACKUP_EXTENSION);

   /* Build final path */
   fill_pathname_join_special(backup_path, backup_dir,
         backup_filename, len);

   return true;
}

/* Returns detected type of specified core backup file */
enum core_backup_type core_backup_get_backup_type(const char *backup_path)
{
   const char *backup_ext            = NULL;
   struct string_list *metadata_list = NULL;
   char core_ext[255];

   core_ext[0] = '\0';

   if (string_is_empty(backup_path) || !path_is_valid(backup_path))
      goto error;

   /* Get backup file extension */
   backup_ext = path_get_extension(backup_path);

   if (string_is_empty(backup_ext))
      goto error;

   /* Get platform-specific dynamic library extension */
   if (!frontend_driver_get_core_extension(core_ext, sizeof(core_ext)))
      goto error;

   /* Check if this is an archived backup */
   if (string_is_equal_noncase(backup_ext,
         FILE_PATH_CORE_BACKUP_EXTENSION_NO_DOT))
   {
      const char *backup_filename = NULL;
      const char *src_ext         = NULL;

      /* Split the backup filename into its various
       * metadata components */
      backup_filename = path_basename(backup_path);

      if (string_is_empty(backup_filename))
         goto error;

      metadata_list = string_split(backup_filename, ".");

      if (!metadata_list || (metadata_list->size != 6))
         goto error;

      /* Get extension of source core file */
      src_ext = metadata_list->elems[1].data;

      if (string_is_empty(src_ext))
         goto error;

      /* Check whether extension is valid */
      if (!string_is_equal_noncase(src_ext, core_ext))
         goto error;

      string_list_free(metadata_list);
      metadata_list = NULL;
   
      return CORE_BACKUP_TYPE_ARCHIVE;
   }

   /* Check if this is a plain dynamic library file */
   if (string_is_equal_noncase(backup_ext, core_ext))
      return CORE_BACKUP_TYPE_LIB;

error:
   if (metadata_list)
   {
      string_list_free(metadata_list);
      metadata_list = NULL;
   }

   return CORE_BACKUP_TYPE_INVALID;
}

/* Fetches crc value of specified core backup file.
 * Returns true if successful */
bool core_backup_get_backup_crc(char *backup_path, uint32_t *crc)
{
   struct string_list *metadata_list = NULL;
   enum core_backup_type backup_type;

   if (string_is_empty(backup_path) || !crc)
      goto error;

   /* Get backup type */
   backup_type = core_backup_get_backup_type(backup_path);

   switch (backup_type)
   {
      case CORE_BACKUP_TYPE_ARCHIVE:
         {
            const char *backup_filename = NULL;
            const char *crc_str         = NULL;

            /* Split the backup filename into its various
             * metadata components */
            backup_filename = path_basename(backup_path);

            if (string_is_empty(backup_filename))
               goto error;

            metadata_list = string_split(backup_filename, ".");

            if (!metadata_list || (metadata_list->size != 6))
               goto error;

            /* Get crc string */
            crc_str = metadata_list->elems[3].data;

            if (string_is_empty(crc_str))
               goto error;

            /* Convert to an integer */
            if ((*crc = (uint32_t)string_hex_to_unsigned(crc_str)) == 0)
               goto error;

            string_list_free(metadata_list);
            metadata_list = NULL;

         }
         return true;
      case CORE_BACKUP_TYPE_LIB:
         {
            intfstream_t *backup_file = NULL;

            /* This is a plain dynamic library file,
             * have to read file data to determine crc */

            /* Open backup file */
            backup_file = intfstream_open_file(
                  backup_path, RETRO_VFS_FILE_ACCESS_READ,
                  RETRO_VFS_FILE_ACCESS_HINT_NONE);

            if (backup_file)
            {
               bool success;

               /* Get crc value */
               success = intfstream_get_crc(backup_file, crc);

               /* Close backup file */
               intfstream_close(backup_file);
               free(backup_file);
               backup_file = NULL;

               return success;
            }
         }
         break;
      default:
         /* Backup is invalid */
         break;
   }

error:
   if (metadata_list)
   {
      string_list_free(metadata_list);
      metadata_list = NULL;
   }

   return false;
}

/* Fetches core path associated with specified core
 * backup file. Returns detected type of backup
 * file - CORE_BACKUP_TYPE_INVALID indicates that
 * backup file cannot be restored/installed, or
 * arguments are otherwise invalid */
enum core_backup_type core_backup_get_core_path(
      const char *backup_path, const char *dir_libretro,
      char *core_path, size_t len)
{
   const char *backup_filename       = NULL;
   char *core_filename               = NULL;
   enum core_backup_type backup_type = CORE_BACKUP_TYPE_INVALID;

   if (string_is_empty(backup_path) || string_is_empty(dir_libretro))
      return backup_type;

   backup_filename = path_basename(backup_path);

   if (string_is_empty(backup_filename))
      return backup_type;

   /* Check backup type */
   switch (core_backup_get_backup_type(backup_path))
   {
      case CORE_BACKUP_TYPE_ARCHIVE:
         {
            char *period  = NULL;

            /* This is an archived backup with timestamp/crc
             * metadata in the filename */
            core_filename = strdup(backup_filename);

            /* Find the location of the second period */
            period = strchr(core_filename, '.');
            if (!period || (*(++period) == '\0'))
               break;

            period = strchr(period, '.');
            if (!period)
               break;

            /* Trim everything after (and including) the
             * second period */
            *period = '\0';

            if (string_is_empty(core_filename))
               break;

            /* All good - build core path */
            fill_pathname_join_special(core_path, dir_libretro,
                  core_filename, len);

            backup_type = CORE_BACKUP_TYPE_ARCHIVE;
         }
         break;
      case CORE_BACKUP_TYPE_LIB:
         /* This is a plain dynamic library file */
         fill_pathname_join_special(core_path, dir_libretro,
               backup_filename, len);
         backup_type = CORE_BACKUP_TYPE_LIB;
         break;
      default:
         /* Backup is invalid */
         break;
   }

   if (core_filename)
   {
      free(core_filename);
      core_filename = NULL;
   }

   return backup_type;
}

/*************************/
/* Backup List Functions */
/*************************/

/**************************************/
/* Initialisation / De-Initialisation */
/**************************************/

/* Parses backup file name and adds to backup list, if valid */
static bool core_backup_add_entry(core_backup_list_t *backup_list,
      const char *core_filename, const char *backup_path)
{
   char *backup_filename           = NULL;
   core_backup_list_entry_t *entry = NULL;
   unsigned long crc               = 0;
   unsigned backup_mode            = 0;

   if (!backup_list ||
       string_is_empty(core_filename) ||
       string_is_empty(backup_path) ||
       (backup_list->size >= backup_list->capacity))
      goto error;

   backup_filename = strdup(path_basename(backup_path));

   if (string_is_empty(backup_filename))
      goto error;

   /* Ensure base backup filename matches core */
   if (!string_starts_with(backup_filename, core_filename))
      goto error;

   /* Remove backup file extension */
   path_remove_extension(backup_filename);

   /* Parse backup filename metadata
    * - <core_filename>.<timestamp>.<crc>.<backup_mode>
    * - timestamp: YYYYMMDDTHHMMSS */
   entry = &backup_list->entries[backup_list->size];

   if (sscanf(backup_filename + strlen(core_filename),
       ".%04u%02u%02uT%02u%02u%02u.%08lx.%u",
       &entry->date.year, &entry->date.month, &entry->date.day,
       &entry->date.hour, &entry->date.minute, &entry->date.second,
       &crc, &backup_mode) != 8)
      goto error;

   entry->crc         = (uint32_t)crc;
   entry->backup_mode = (enum core_backup_mode)backup_mode;

   /* Cache backup path */
   entry->backup_path = strdup(backup_path);
   backup_list->size++;

   free(backup_filename);

   return true;

error:
   if (backup_filename)
      free(backup_filename);

   return false;
}

/* Creates a new core backup list containing entries
 * for all existing backup files.
 * Returns a handle to a new core_backup_list_t object
 * on success, otherwise returns NULL. */
core_backup_list_t *core_backup_list_init(
      const char *core_path, const char *dir_core_assets)
{
   size_t i;
   const char *core_filename         = NULL;
   struct string_list *dir_list      = NULL;
   core_backup_list_t *backup_list   = NULL;
   core_backup_list_entry_t *entries = NULL;
   char core_dir[PATH_MAX_LENGTH];
   char backup_dir[PATH_MAX_LENGTH];

   core_dir[0]   = '\0'; 
   backup_dir[0] = '\0';

   /* Get core filename and parent directory */
   if (string_is_empty(core_path))
      goto error;

   core_filename = path_basename(core_path);

   if (string_is_empty(core_filename))
      goto error;

   fill_pathname_parent_dir(core_dir, core_path, sizeof(core_dir));

   if (string_is_empty(core_dir))
      goto error;

   /* Get backup directory */
   if (!core_backup_get_backup_dir(core_dir, dir_core_assets, core_filename,
         backup_dir, sizeof(backup_dir)))
      goto error;

   /* Get backup file list */
   dir_list = dir_list_new(
         backup_dir,
         FILE_PATH_CORE_BACKUP_EXTENSION,
         false, /* include_dirs */
         false, /* include_hidden */
         false, /* include_compressed */
         false  /* recursive */
   );

   /* Sanity check */
   if (!dir_list)
      goto error;

   if (dir_list->size < 1)
      goto error;

   /* Ensure list is sorted in alphabetical order
    * > This corresponds to 'timestamp' order */
   dir_list_sort(dir_list, true);

   /* Create core backup list */
   backup_list = (core_backup_list_t*)malloc(sizeof(*backup_list));

   if (!backup_list)
      goto error;

   backup_list->entries  = NULL;
   backup_list->capacity = 0;
   backup_list->size     = 0;

   /* Create entries array
    * (Note: Set this to the full size of the directory
    * list - this may be larger than we need, but saves
    * many inefficiencies later)   */
   entries               = (core_backup_list_entry_t*)
      calloc(dir_list->size, sizeof(*entries));

   if (!entries)
      goto error;

   backup_list->entries  = entries;
   backup_list->capacity = dir_list->size;

   /* Loop over backup files and parse file names */
   for (i = 0; i < dir_list->size; i++)
   {
      const char *backup_path = dir_list->elems[i].data;
      core_backup_add_entry(backup_list, core_filename, backup_path);
   }

   if (backup_list->size == 0)
      goto error;

   string_list_free(dir_list);

   return backup_list;

error:
   if (dir_list)
      string_list_free(dir_list);

   if (backup_list)
      core_backup_list_free(backup_list);

   return NULL;
}

/* Frees specified core backup list */
void core_backup_list_free(core_backup_list_t *backup_list)
{
   size_t i;

   if (!backup_list)
      return;

   if (backup_list->entries)
   {
      for (i = 0; i < backup_list->size; i++)
      {
         core_backup_list_entry_t *entry = &backup_list->entries[i];

         if (!entry)
            continue;

         if (entry->backup_path)
         {
            free(entry->backup_path);
            entry->backup_path = NULL;
         }
      }

      free(backup_list->entries);
      backup_list->entries = NULL;
   }

   free(backup_list);
}

/***********/
/* Getters */
/***********/

/* Returns number of entries in core backup list */
size_t core_backup_list_size(core_backup_list_t *backup_list)
{
   if (!backup_list)
      return 0;

   return backup_list->size;
}

/* Returns number of entries of specified 'backup mode'
 * (manual or automatic) in core backup list */
size_t core_backup_list_get_num_backups(
      core_backup_list_t *backup_list,
      enum core_backup_mode backup_mode)
{
   size_t i;
   size_t num_backups = 0;

   if (!backup_list || !backup_list->entries)
      return 0;

   for (i = 0; i < backup_list->size; i++)
   {
      core_backup_list_entry_t *current_entry = &backup_list->entries[i];

      if (current_entry &&
          (current_entry->backup_mode == backup_mode))
         num_backups++;
   }

   return num_backups;
}

/* Fetches core backup list entry corresponding
 * to the specified entry index.
 * Returns false if index is invalid. */
bool core_backup_list_get_index(
      core_backup_list_t *backup_list,
      size_t idx,
      const core_backup_list_entry_t **entry)
{
   if (!backup_list || !backup_list->entries || !entry)
      return false;

   if (idx >= backup_list->size)
      return false;

   *entry = &backup_list->entries[idx];

   if (*entry)
      return true;

   return false;
}

/* Fetches core backup list entry corresponding
 * to the specified core crc checksum value.
 * Note that 'manual' and 'auto' backups are
 * considered independent - we only compare
 * crc values for the specified backup_mode.
 * Returns false if entry is not found. */
bool core_backup_list_get_crc(
      core_backup_list_t *backup_list,
      uint32_t crc, enum core_backup_mode backup_mode,
      const core_backup_list_entry_t **entry)
{
   size_t i;

   if (!backup_list || !backup_list->entries || !entry)
      return false;

   for (i = 0; i < backup_list->size; i++)
   {
      core_backup_list_entry_t *current_entry = &backup_list->entries[i];

      if (current_entry &&
          (current_entry->crc == crc) &&
          (current_entry->backup_mode == backup_mode))
      {
         *entry = current_entry;
         return true;
      }
   }

   return false;
}

/* Fetches a string representation of a backup
 * list entry timestamp.
 * Returns false in the event of an error */
bool core_backup_list_get_entry_timestamp_str(
      const core_backup_list_entry_t *entry,
      enum core_backup_date_separator_type date_separator,
      char *timestamp, size_t len)
{
   const char *format_str = "";

   if (!entry || (len < 20))
      return false;

   /* Get time format string */
   switch (date_separator)
   {
      case CORE_BACKUP_DATE_SEPARATOR_SLASH:
         format_str = "%04u/%02u/%02u %02u:%02u:%02u";
         break;
      case CORE_BACKUP_DATE_SEPARATOR_PERIOD:
         format_str = "%04u.%02u.%02u %02u:%02u:%02u";
         break;
      default:
         format_str = "%04u-%02u-%02u %02u:%02u:%02u";
         break;
   }

   snprintf(timestamp, len,
         format_str,
         entry->date.year,
         entry->date.month,
         entry->date.day,
         entry->date.hour,
         entry->date.minute,
         entry->date.second);

   return true;
}

/* Fetches a string representation of a backup
 * list entry crc value.
 * Returns false in the event of an error */
bool core_backup_list_get_entry_crc_str(
      const core_backup_list_entry_t *entry,
      char *crc, size_t len)
{
   if (!entry || (len < 9))
      return false;

   snprintf(crc, len, "%08lx", (unsigned long)entry->crc);
   return true;
}
