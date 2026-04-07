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
      char *s, size_t len)
{
   char tmp[PATH_MAX_LENGTH];
   char core_file_id[NAME_MAX_LENGTH];
   char *last_underscore = NULL;

   /* Extract core file 'ID' (name without extension + suffix)
    * from core path */
   if (   (!dir_libretro || !*dir_libretro)
       || (!core_filename || !*core_filename)
       || (len < 1))
      return false;

   fill_pathname(core_file_id, core_filename, "",
         sizeof(core_file_id));
   if (!*core_file_id)
      return false;

   /* > Remove platform-specific file name suffix,
    *   if required */
   last_underscore = strrchr(core_file_id, '_');

   if (last_underscore && *last_underscore)
      if (!string_is_equal(last_underscore, "_libretro"))
         *last_underscore = '\0';

   if (!*core_file_id)
      return false;

   /* Get core backup directory
    * > If no assets directory is defined, use
    *   core directory as a base */
   fill_pathname_join_special(tmp,
         (!dir_core_assets || !*dir_core_assets)
         ? dir_libretro
         : dir_core_assets,
               "core_backups", sizeof(tmp));

   fill_pathname_join_special(s, tmp,
         core_file_id, len);

   if (!s || !*s)
      return false;

   /* > Create directory, if required */
   if (!path_is_directory(s))
   {
      if (!path_mkdir(s))
      {
         RARCH_ERR("[Core backup] Failed to create backup directory: %s.\n", s);
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
      char *s, size_t len)
{
   time_t current_time;
   struct tm time_info;
   const char *core_filename = NULL;
   char core_dir[DIR_MAX_LENGTH];
   char backup_dir[DIR_MAX_LENGTH];
   char backup_filename[PATH_MAX_LENGTH];

   backup_dir[0]      = '\0';
   backup_filename[0] = '\0';
   /* Get core filename and parent directory */
   if (!core_path || !*core_path)
      return false;
   core_filename = path_basename(core_path);
   if (!core_filename || !*core_filename)
      return false;
   fill_pathname_parent_dir(core_dir, core_path, sizeof(core_dir));
   if (!*core_dir)
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
   fill_pathname_join_special(s, backup_dir,
         backup_filename, len);

   return true;
}

/* Returns detected type of specified core backup file */
enum core_backup_type core_backup_get_backup_type(const char *backup_path)
{
   char core_ext[16];
   const char *backup_ext            = NULL;
   if ((!backup_path || !*backup_path) || !path_is_valid(backup_path))
      return CORE_BACKUP_TYPE_INVALID;
   /* Get backup file extension */
   backup_ext = path_get_extension(backup_path);
   if (backup_ext && *backup_ext)
   {
      /* Get platform-specific dynamic library extension */
      if (frontend_driver_get_core_extension(core_ext, sizeof(core_ext)))
      {
         /* Check if this is an archived backup */
         if (string_is_equal_noncase(backup_ext,
                  FILE_PATH_CORE_BACKUP_EXTENSION_NO_DOT))
         {
            bool ret = false;
            struct string_list *metadata_list = NULL;
            const char *src_ext         = NULL;
            /* Split the backup filename into its various
             * metadata components */
            const char *backup_filename = path_basename(backup_path);
            if (!backup_filename || !*backup_filename)
               return CORE_BACKUP_TYPE_INVALID;
            metadata_list = string_split(backup_filename, ".");
            if (!metadata_list)
               return CORE_BACKUP_TYPE_INVALID;
            if (metadata_list->size != 6)
            {
               string_list_free(metadata_list);
               metadata_list = NULL;
               return CORE_BACKUP_TYPE_INVALID;
            }
            /* Get extension of source core file */
            src_ext = metadata_list->elems[1].data;
            ret     = (!src_ext || !*src_ext)
               || !string_is_equal_noncase(src_ext, core_ext);
            string_list_free(metadata_list);
            metadata_list = NULL;
            /* Check whether extension is valid */
            if (ret)
               return CORE_BACKUP_TYPE_INVALID;
            return CORE_BACKUP_TYPE_ARCHIVE;
         }
         /* Check if this is a plain dynamic library file */
         if (string_is_equal_noncase(backup_ext, core_ext))
            return CORE_BACKUP_TYPE_LIB;
      }
   }
   return CORE_BACKUP_TYPE_INVALID;
}

/* Fetches crc value of specified core backup file.
 * Returns true if successful */
bool core_backup_get_backup_crc(char *s, uint32_t *crc)
{
   enum core_backup_type backup_type;
   if ((!s || !*s) || !crc)
      return false;
   /* Get backup type */
   backup_type = core_backup_get_backup_type(s);
   switch (backup_type)
   {
      case CORE_BACKUP_TYPE_ARCHIVE:
         {
            uint32_t val;
            struct string_list *metadata_list = NULL;
            bool ret                    = false;
            const char *crc_str         = NULL;
            /* Split the backup filename into its various
             * metadata components */
            const char *backup_filename = path_basename(s);
            if (!backup_filename || !*backup_filename)
               return false;
            metadata_list = string_split(backup_filename, ".");
            if (!metadata_list)
               return false;
            if (metadata_list->size != 6)
            {
               string_list_free(metadata_list);
               metadata_list = NULL;
               return false;
            }

            /* Get crc string */
            crc_str = metadata_list->elems[3].data;
            ret     = !crc_str || !*crc_str;
            /* Convert to an integer */
            val     = (uint32_t)string_hex_to_unsigned(crc_str);

            string_list_free(metadata_list);
            metadata_list = NULL;

            if (ret || ((*crc = val) == 0))
               return false;
         }
         return true;
      case CORE_BACKUP_TYPE_LIB:
         {
            intfstream_t *backup_file = NULL;

            /* This is a plain dynamic library file,
             * have to read file data to determine crc */

            /* Open backup file */
            backup_file = intfstream_open_file(
                  s, RETRO_VFS_FILE_ACCESS_READ,
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

   return false;
}

/* Fetches core path associated with specified core
 * backup file. Returns detected type of backup
 * file - CORE_BACKUP_TYPE_INVALID indicates that
 * backup file cannot be restored/installed, or
 * arguments are otherwise invalid */
enum core_backup_type core_backup_get_core_path(
      const char *backup_path, const char *dir_libretro,
      char *s, size_t len)
{
   const char *backup_filename       = NULL;
   if ((!backup_path || !*backup_path) || (!dir_libretro || !*dir_libretro))
      return CORE_BACKUP_TYPE_INVALID;
   backup_filename = path_basename(backup_path);
   if (backup_filename && *backup_filename)
   {
      /* Check backup type */
      switch (core_backup_get_backup_type(backup_path))
      {
         case CORE_BACKUP_TYPE_ARCHIVE:
            {
               /* This is an archived backup with timestamp/crc
                * metadata in the filename */
               size_t dir_len;
               size_t core_len;
               const char *period = strchr(backup_filename, '.');
               if (!period || *(++period) == '\0')
                  break;
               period = strchr(period, '.');
               if (!period)
                  break;
               /* Length of the core filename up to (but not
                * including) the second period */
               core_len = (size_t)(period - backup_filename);
               if (core_len == 0)
                  break;
               /* Build core path: dir_libretro / core_filename */
               dir_len = strlen(dir_libretro);
               if (dir_len + core_len + 1 >= len)
                  break;
               memcpy(s, dir_libretro, dir_len);
               s[dir_len]   = PATH_DEFAULT_SLASH_C();
               memcpy(s + dir_len + 1, backup_filename, core_len);
               s[dir_len + 1 + core_len] = '\0';
            }
            return CORE_BACKUP_TYPE_ARCHIVE;
         case CORE_BACKUP_TYPE_LIB:
            /* This is a plain dynamic library file */
            fill_pathname_join_special(s, dir_libretro,
                  backup_filename, len);
            return CORE_BACKUP_TYPE_LIB;
         default:
            /* Backup is invalid */
            break;
      }
   }
   return CORE_BACKUP_TYPE_INVALID;
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
   const char *p                   = NULL;
   char *endptr                    = NULL;
   unsigned long crc               = 0;
   unsigned backup_mode            = 0;

   if (  !backup_list
       || (!core_filename || !*core_filename)
       || (!backup_path || !*backup_path)
       || (backup_list->size >= backup_list->capacity))
      return false;

   backup_filename = strdup(path_basename(backup_path));
   if (!backup_filename || !*backup_filename)
      return false;

   /* Ensure base backup filename matches core */
   if (!string_starts_with(backup_filename, core_filename))
   {
      free(backup_filename);
      return false;
   }

   /* Remove backup file extension */
   path_remove_extension(backup_filename);

   /* Parse backup filename metadata
    * - <core_filename>.<timestamp>.<crc>.<backup_mode>
    * - timestamp: YYYYMMDDTHHMMSS */
   entry = &backup_list->entries[backup_list->size];

   p = backup_filename + strlen(core_filename);

   /* Expect '.' separator before timestamp */
   if (*p != '.')
   {
      free(backup_filename);
      return false;
   }
   p++;

   /* Timestamp must be exactly 15 characters: YYYYMMDDTHHMMSS */
   if (strlen(p) < 15 || p[8] != 'T')
   {
      free(backup_filename);
      return false;
   }

   /* Parse date/time components */
   {
      char buf[5];

      /* Year (4 digits) */
      memcpy(buf, p, 4);
      buf[4] = '\0';
      entry->date.year = (unsigned)strtoul(buf, &endptr, 10);
      if (*endptr != '\0')
      {
         free(backup_filename);
         return false;
      }
      p += 4;

      /* Month (2 digits) */
      memcpy(buf, p, 2);
      buf[2] = '\0';
      entry->date.month = (unsigned)strtoul(buf, &endptr, 10);
      if (*endptr != '\0')
      {
         free(backup_filename);
         return false;
      }
      p += 2;

      /* Day (2 digits) */
      memcpy(buf, p, 2);
      buf[2] = '\0';
      entry->date.day = (unsigned)strtoul(buf, &endptr, 10);
      if (*endptr != '\0')
      {
         free(backup_filename);
         return false;
      }
      p += 2;

      /* Skip 'T' separator */
      p++;

      /* Hour (2 digits) */
      memcpy(buf, p, 2);
      buf[2] = '\0';
      entry->date.hour = (unsigned)strtoul(buf, &endptr, 10);
      if (*endptr != '\0')
      {
         free(backup_filename);
         return false;
      }
      p += 2;

      /* Minute (2 digits) */
      memcpy(buf, p, 2);
      buf[2] = '\0';
      entry->date.minute = (unsigned)strtoul(buf, &endptr, 10);
      if (*endptr != '\0')
      {
         free(backup_filename);
         return false;
      }
      p += 2;

      /* Second (2 digits) */
      memcpy(buf, p, 2);
      buf[2] = '\0';
      entry->date.second = (unsigned)strtoul(buf, &endptr, 10);
      if (*endptr != '\0')
      {
         free(backup_filename);
         return false;
      }
      p += 2;
   }

   /* Expect '.' separator before CRC */
   if (*p != '.')
   {
      free(backup_filename);
      return false;
   }
   p++;

   /* Parse 8-character hex CRC */
   crc = strtoul(p, &endptr, 16);
   if (endptr != p + 8 || *endptr != '.')
   {
      free(backup_filename);
      return false;
   }
   p = endptr + 1;

   /* Parse backup mode */
   backup_mode = (unsigned)strtoul(p, &endptr, 10);
   if (endptr == p || *endptr != '\0')
   {
      free(backup_filename);
      return false;
   }

   entry->crc         = (uint32_t)crc;
   entry->backup_mode = (enum core_backup_mode)backup_mode;

   /* Cache backup path */
   entry->backup_path = strdup(backup_path);
   backup_list->size++;

   free(backup_filename);
   return true;
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
   char core_dir[DIR_MAX_LENGTH];
   char backup_dir[DIR_MAX_LENGTH];
   core_dir[0]   = '\0';
   backup_dir[0] = '\0';
   /* Get core filename and parent directory */
   if (!core_path || !*core_path)
      return NULL;
   core_filename = path_basename(core_path);
   if (!core_filename || !*core_filename)
      return NULL;
   fill_pathname_parent_dir(core_dir, core_path, sizeof(core_dir));
   if (!*core_dir)
      return NULL;
   /* Get backup directory */
   if (!core_backup_get_backup_dir(core_dir, dir_core_assets, core_filename,
         backup_dir, sizeof(backup_dir)))
      return NULL;

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
      return NULL;

   if (dir_list->size < 1)
   {
      string_list_free(dir_list);
      return NULL;
   }

   /* Ensure list is sorted in alphabetical order
    * > This corresponds to 'timestamp' order */
   dir_list_sort(dir_list, true);

   /* Create entries array
    * (Note: Set this to the full size of the directory
    * list - this may be larger than we need, but saves
    * many inefficiencies later)   */
   if (!(entries = (core_backup_list_entry_t*)
      calloc(dir_list->size, sizeof(*entries))))
   {
      string_list_free(dir_list);
      return NULL;
   }

   /* Create core backup list */
   if (!(backup_list = (core_backup_list_t*)malloc(sizeof(*backup_list))))
   {
      string_list_free(dir_list);
      return NULL;
   }

   backup_list->entries  = NULL;
   backup_list->capacity = 0;
   backup_list->size     = 0;

   backup_list->entries  = entries;
   backup_list->capacity = dir_list->size;

   /* Loop over backup files and parse file names */
   for (i = 0; i < dir_list->size; i++)
   {
      const char *backup_path = dir_list->elems[i].data;
      core_backup_add_entry(backup_list, core_filename, backup_path);
   }

   string_list_free(dir_list);

   if (backup_list->size != 0)
      return backup_list;

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
      if (    current_entry
          && (current_entry->backup_mode == backup_mode))
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

      if (   (current_entry)
          && (current_entry->crc == crc)
          && (current_entry->backup_mode == backup_mode))
      {
         *entry = current_entry;
         return true;
      }
   }

   return false;
}
