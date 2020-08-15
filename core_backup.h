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

#ifndef __CORE_BACKUP_H
#define __CORE_BACKUP_H

#include <retro_common_api.h>
#include <libretro.h>

#include <boolean.h>

RETRO_BEGIN_DECLS

/* Defines the various types of supported core backup
 * file. Allows us to handle manual core installs
 * (via downloaded/compiled dynamic libraries dropped
 * in the 'downloads' folder) using the same task
 * interface as 'managed'/archived backups */
enum core_backup_type
{
   CORE_BACKUP_TYPE_INVALID = 0,
   CORE_BACKUP_TYPE_ARCHIVE,
   CORE_BACKUP_TYPE_LIB
};

/* Used to distinguish manual and automatic
 * core backups */
enum core_backup_mode
{
   CORE_BACKUP_MODE_MANUAL = 0,
   CORE_BACKUP_MODE_AUTO
};

/* Note: These must be kept synchronised with
 * 'enum menu_timedate_date_separator_type' in
 * 'menu_defines.h' */
enum core_backup_date_separator_type
{
   CORE_BACKUP_DATE_SEPARATOR_HYPHEN = 0,
   CORE_BACKUP_DATE_SEPARATOR_SLASH,
   CORE_BACKUP_DATE_SEPARATOR_PERIOD,
   CORE_BACKUP_DATE_SEPARATOR_LAST
};

/* Holds all timestamp info for a core backup file */
typedef struct
{
   unsigned year;
   unsigned month;
   unsigned day;
   unsigned hour;
   unsigned minute;
   unsigned second;
} core_backup_list_date_t;

/* Holds all info related to a core backup file */
typedef struct
{
   char *backup_path;
   core_backup_list_date_t date;       /* unsigned alignment */
   uint32_t crc;
   enum core_backup_mode backup_mode;
} core_backup_list_entry_t;

/* Prevent direct access to core_backup_list_t
 * members */
typedef struct core_backup_list core_backup_list_t;

/*********************/
/* Utility Functions */
/*********************/

/* Generates a timestamped core backup file path from
 * the specified core path. Returns true if successful */
bool core_backup_get_backup_path(
      const char *core_path, uint32_t crc, enum core_backup_mode backup_mode,
      const char *dir_core_assets, char *backup_path, size_t len);

/* Returns detected type of specified core backup file */
enum core_backup_type core_backup_get_backup_type(const char *backup_path);

/* Fetches crc value of specified core backup file.
 * Returns true if successful */
bool core_backup_get_backup_crc(char *backup_path, uint32_t *crc);

/* Fetches core path associated with specified core
 * backup file. Returns detected type of backup
 * file - CORE_BACKUP_TYPE_INVALID indicates that
 * backup file cannot be restored/installed, or
 * arguments are otherwise invalid */
enum core_backup_type core_backup_get_core_path(
      const char *backup_path, const char *dir_libretro,
      char *core_path, size_t len);

/*************************/
/* Backup List Functions */
/*************************/

/**************************************/
/* Initialisation / De-Initialisation */
/**************************************/

/* Creates a new core backup list containing entries
 * for all existing backup files.
 * Returns a handle to a new core_backup_list_t object
 * on success, otherwise returns NULL. */
core_backup_list_t *core_backup_list_init(
      const char *core_path, const char *dir_core_assets);

/* Frees specified core backup list */
void core_backup_list_free(core_backup_list_t *backup_list);

/***********/
/* Getters */
/***********/

/* Returns number of entries in core backup list */
size_t core_backup_list_size(core_backup_list_t *backup_list);

/* Returns number of entries of specified 'backup mode'
 * (manual or automatic) in core backup list */
size_t core_backup_list_get_num_backups(
      core_backup_list_t *backup_list,
      enum core_backup_mode backup_mode);

/* Fetches core backup list entry corresponding
 * to the specified entry index.
 * Returns false if index is invalid. */
bool core_backup_list_get_index(
      core_backup_list_t *backup_list,
      size_t idx,
      const core_backup_list_entry_t **entry);

/* Fetches core backup list entry corresponding
 * to the specified core crc checksum value.
 * Note that 'manual' and 'auto' backups are
 * considered independent - we only compare
 * crc values for the specified backup_mode.
 * Returns false if entry is not found. */
bool core_backup_list_get_crc(
      core_backup_list_t *backup_list,
      uint32_t crc, enum core_backup_mode backup_mode,
      const core_backup_list_entry_t **entry);

/* Fetches a string representation of a backup
 * list entry timestamp.
 * Returns false in the event of an error */
bool core_backup_list_get_entry_timestamp_str(
      const core_backup_list_entry_t *entry,
      enum core_backup_date_separator_type date_separator,
      char *timestamp, size_t len);

/* Fetches a string representation of a backup
 * list entry crc value.
 * Returns false in the event of an error */
bool core_backup_list_get_entry_crc_str(
      const core_backup_list_entry_t *entry,
      char *crc, size_t len);

RETRO_END_DECLS

#endif
