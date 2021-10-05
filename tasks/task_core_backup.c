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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <boolean.h>

#include <string/stdstring.h>
#include <file/file_path.h>
#include <streams/interface_stream.h>
#include <streams/file_stream.h>
#include <streams/rzip_stream.h>

#include "../retroarch.h"
#include "../paths.h"
#include "../command.h"
#include "../msg_hash.h"
#include "../verbosity.h"
#include "../core_info.h"
#include "../core_backup.h"

#if defined(ANDROID)
#include "../play_feature_delivery/play_feature_delivery.h"
#endif

#define CORE_BACKUP_CHUNK_SIZE 4096

enum core_backup_status
{
   CORE_BACKUP_BEGIN = 0,
   CORE_BACKUP_CHECK_CRC,
   CORE_BACKUP_PRE_ITERATE,
   CORE_BACKUP_ITERATE,
   CORE_BACKUP_CHECK_HISTORY,
   CORE_BACKUP_PRUNE_HISTORY,
   CORE_BACKUP_END,
   CORE_RESTORE_GET_CORE_CRC,
   CORE_RESTORE_GET_BACKUP_CRC,
   CORE_RESTORE_CHECK_CRC,
   CORE_RESTORE_PRE_ITERATE,
   CORE_RESTORE_ITERATE,
   CORE_RESTORE_END
};

typedef struct core_backup_handle
{
   int64_t core_file_size;
   int64_t backup_file_size;
   int64_t file_data_read;
   char *dir_core_assets;
   char *core_path;
   char *core_name;
   char *backup_path;
   intfstream_t *core_file;
   intfstream_t *backup_file;
   core_backup_list_t *backup_list;
   size_t auto_backup_history_size;
   size_t num_auto_backups_to_remove;
   size_t backup_index;
   uint32_t core_crc;
   uint32_t backup_crc;
   enum core_backup_type backup_type;
   enum core_backup_mode backup_mode;
   enum core_backup_status status;
   bool crc_match;
   bool success;
} core_backup_handle_t;

/*********************/
/* Utility functions */
/*********************/

static void free_core_backup_handle(core_backup_handle_t *backup_handle)
{
   if (!backup_handle)
      return;

   if (backup_handle->dir_core_assets)
   {
      free(backup_handle->dir_core_assets);
      backup_handle->dir_core_assets = NULL;
   }

   if (backup_handle->core_path)
   {
      free(backup_handle->core_path);
      backup_handle->core_path = NULL;
   }

   if (backup_handle->core_name)
   {
      free(backup_handle->core_name);
      backup_handle->core_name = NULL;
   }

   if (backup_handle->backup_path)
   {
      free(backup_handle->backup_path);
      backup_handle->backup_path = NULL;
   }

   if (backup_handle->core_file)
   {
      intfstream_close(backup_handle->core_file);
      free(backup_handle->core_file);
      backup_handle->core_file = NULL;
   }

   if (backup_handle->backup_file)
   {
      intfstream_close(backup_handle->backup_file);
      free(backup_handle->backup_file);
      backup_handle->backup_file = NULL;
   }

   if (backup_handle->backup_list)
   {
      core_backup_list_free(backup_handle->backup_list);
      backup_handle->backup_list = NULL;
   }

   free(backup_handle);
   backup_handle = NULL;
}

/* Forward declarations, required for task_core_backup_finder() */
static void task_core_backup_handler(retro_task_t *task);

static void task_core_restore_handler(retro_task_t *task);

static bool task_core_backup_finder(retro_task_t *task, void *user_data)
{
   core_backup_handle_t *backup_handle = NULL;
   const char *core_filename_a         = NULL;
   const char *core_filename_b         = NULL;

   if (!task || !user_data)
      return false;

   if ((task->handler != task_core_backup_handler) &&
       (task->handler != task_core_restore_handler))
      return false;

   backup_handle = (core_backup_handle_t*)task->state;
   if (!backup_handle)
      return false;

   if (string_is_empty(backup_handle->core_path))
      return false;

   core_filename_a = path_basename((const char*)user_data);
   core_filename_b = path_basename(backup_handle->core_path);

   if (string_is_empty(core_filename_a) ||
       string_is_empty(core_filename_b))
      return false;

   return string_is_equal(core_filename_a, core_filename_b);
}

/***************/
/* Core Backup */
/***************/

static void task_core_backup_handler(retro_task_t *task)
{
   core_backup_handle_t *backup_handle = NULL;

   if (!task)
      goto task_finished;

   backup_handle = (core_backup_handle_t*)task->state;

   if (!backup_handle)
      goto task_finished;

   if (task_get_cancelled(task))
      goto task_finished;

   switch (backup_handle->status)
   {
      case CORE_BACKUP_BEGIN:
         /* Get current list of backups */
         backup_handle->backup_list = core_backup_list_init(
               backup_handle->core_path, backup_handle->dir_core_assets);

         /* Open core file */
         backup_handle->core_file = intfstream_open_file(
               backup_handle->core_path,
               RETRO_VFS_FILE_ACCESS_READ,
               RETRO_VFS_FILE_ACCESS_HINT_NONE);

         if (!backup_handle->core_file)
         {
            RARCH_ERR("[core backup] Failed to open core file: %s\n",
                  backup_handle->core_path);
            backup_handle->status = CORE_BACKUP_END;
            break;
         }

         /* Get core file size */
         backup_handle->core_file_size = intfstream_get_size(backup_handle->core_file);

         if (backup_handle->core_file_size <= 0)
         {
            RARCH_ERR("[core backup] Core file is empty/invalid: %s\n",
                  backup_handle->core_path);
            backup_handle->status = CORE_BACKUP_END;
            break;
         }

         /* Go to CRC checking phase */
         backup_handle->status = CORE_BACKUP_CHECK_CRC;
         break;
      case CORE_BACKUP_CHECK_CRC:
         /* Check whether we need to calculate CRC value */
         if (backup_handle->core_crc == 0)
         {
            if (!intfstream_get_crc(backup_handle->core_file,
                     &backup_handle->core_crc))
            {
               RARCH_ERR("[core backup] Failed to determine CRC of core file: %s\n",
                     backup_handle->core_path);
               backup_handle->status = CORE_BACKUP_END;
               break;
            }
         }

         /* Check whether a backup with this CRC already
          * exists */
         if (backup_handle->backup_list)
         {
            const core_backup_list_entry_t *entry = NULL;

            if (core_backup_list_get_crc(
                     backup_handle->backup_list,
                     backup_handle->core_crc,
                     backup_handle->backup_mode,
                     &entry))
            {
               RARCH_LOG("[core backup] Current version of core is already backed up: %s\n",
                     entry->backup_path);

               backup_handle->crc_match = true;
               backup_handle->success   = true;
               backup_handle->status    = CORE_BACKUP_END;
               break;
            }
         }

         /* Go to pre-iteration phase */
         backup_handle->status = CORE_BACKUP_PRE_ITERATE;
         break;
      case CORE_BACKUP_PRE_ITERATE:
         {
            char task_title[PATH_MAX_LENGTH];
            char backup_path[PATH_MAX_LENGTH];

            task_title[0]  = '\0';
            backup_path[0] = '\0';

            /* Get backup path */
            if (!core_backup_get_backup_path(
                  backup_handle->core_path,
                  backup_handle->core_crc,
                  backup_handle->backup_mode,
                  backup_handle->dir_core_assets,
                  backup_path, sizeof(backup_path)))
            {
               RARCH_ERR("[core backup] Failed to generate backup path for core file: %s\n",
                     backup_handle->core_path);
               backup_handle->status = CORE_BACKUP_END;
               break;
            }

            backup_handle->backup_path = strdup(backup_path);

            /* Open backup file */
#if defined(HAVE_ZLIB)
            backup_handle->backup_file = intfstream_open_rzip_file(
                  backup_handle->backup_path, RETRO_VFS_FILE_ACCESS_WRITE);
#else
            backup_handle->backup_file = intfstream_open_file(
                  backup_handle->backup_path, RETRO_VFS_FILE_ACCESS_WRITE,
                  RETRO_VFS_FILE_ACCESS_HINT_NONE);
#endif
            if (!backup_handle->backup_file)
            {
               RARCH_ERR("[core backup] Failed to open core backup file: %s\n",
                     backup_handle->backup_path);
               backup_handle->status = CORE_BACKUP_END;
               break;
            }

            /* Update task title */
            task_free_title(task);
            strlcpy(task_title, msg_hash_to_str(MSG_BACKING_UP_CORE),
                  sizeof(task_title));
            strlcat(task_title, backup_handle->core_name, sizeof(task_title));
            task_set_title(task, strdup(task_title));

            /* Go to iteration phase */
            backup_handle->status = CORE_BACKUP_ITERATE;
         }
         break;
      case CORE_BACKUP_ITERATE:
         {
            int64_t data_written = 0;
            uint8_t buffer[CORE_BACKUP_CHUNK_SIZE];
            /* Read a single chunk from the core file */
            int64_t data_read    = intfstream_read(
                  backup_handle->core_file, buffer, sizeof(buffer));

            if (data_read < 0)
            {
               RARCH_ERR("[core backup] Failed to read from core file: %s\n",
                     backup_handle->core_path);
               backup_handle->status = CORE_BACKUP_END;
               break;
            }

            backup_handle->file_data_read += data_read;

            /* Check whether we have reached the end of the file */
            if (data_read == 0)
            {
               /* Close core file */
               intfstream_close(backup_handle->core_file);
               free(backup_handle->core_file);
               backup_handle->core_file   = NULL;

               /* Close backup file */
               intfstream_flush(backup_handle->backup_file);
               intfstream_close(backup_handle->backup_file);
               free(backup_handle->backup_file);
               backup_handle->backup_file = NULL;

               backup_handle->success = true;

               /* If this is an automatic backup, check whether
                * any old backup files should be deleted.
                * In all other cases, backup is complete */
               backup_handle->status  = (backup_handle->backup_mode ==
                     CORE_BACKUP_MODE_AUTO) ?
                           CORE_BACKUP_CHECK_HISTORY : CORE_BACKUP_END;
               break;
            }

            /* Write chunk to backup file */
            data_written = intfstream_write(backup_handle->backup_file, buffer, data_read);

            if (data_written != data_read)
            {
               RARCH_ERR("[core backup] Failed to write to core backup file: %s\n",
                     backup_handle->backup_path);
               backup_handle->status = CORE_BACKUP_END;
               break;
            }

            /* Update progress display */
            task_set_progress(task,
                  (int8_t)((backup_handle->file_data_read * 100) / backup_handle->core_file_size));
         }
         break;
      case CORE_BACKUP_CHECK_HISTORY:
         {
            size_t num_backups;

            /* Sanity check: We can only reach this stage
             * when performing automatic backups, but verify
             * that this is true (i.e. don't inadvertently
             * delete backups of the wrong type if someone
             * edits the CORE_BACKUP_ITERATE case statement...) */
            if (backup_handle->backup_mode != CORE_BACKUP_MODE_AUTO)
            {
               backup_handle->status = CORE_BACKUP_END;
               break;
            }

            /* This is an automated backup
             * > Fetch number of existing auto backups
             *   in the list
             * > If we reach this stage then a new backup
             *   has been created successfully -> increment
             *   count by one */
            num_backups = core_backup_list_get_num_backups(
                  backup_handle->backup_list,
                  CORE_BACKUP_MODE_AUTO);
            num_backups++;

            /* Check whether we have exceeded the backup
             * history size limit */
            if (num_backups > backup_handle->auto_backup_history_size)
            {
               char task_title[PATH_MAX_LENGTH];

               task_title[0]  = '\0';

               /* Get number of old backups to remove */
               backup_handle->num_auto_backups_to_remove = num_backups -
                     backup_handle->auto_backup_history_size;

               /* Update task title */
               task_free_title(task);
               strlcpy(task_title, msg_hash_to_str(MSG_PRUNING_CORE_BACKUP_HISTORY),
                     sizeof(task_title));
               strlcat(task_title, backup_handle->core_name, sizeof(task_title));
               task_set_title(task, strdup(task_title));

               /* Go to history clean-up phase */
               backup_handle->status = CORE_BACKUP_PRUNE_HISTORY;
            }
            /* No backups to remove - task is complete */
            else
               backup_handle->status = CORE_BACKUP_END;
         }
         break;
      case CORE_BACKUP_PRUNE_HISTORY:
         {
            size_t list_size = core_backup_list_size(backup_handle->backup_list);

            /* The core backup list is automatically sorted
             * by timestamp - simply loop from start to end
             * and delete automatic backups until the required
             * number have been removed */
            while ((backup_handle->backup_index < list_size) &&
                   (backup_handle->num_auto_backups_to_remove > 0))
            {
               const core_backup_list_entry_t *entry = NULL;

               if (core_backup_list_get_index(
                     backup_handle->backup_list,
                     backup_handle->backup_index,
                     &entry) &&
                     entry &&
                     (entry->backup_mode == CORE_BACKUP_MODE_AUTO))
               {
                  /* Delete backup file (if it exists) */
                  if (path_is_valid(entry->backup_path))
                     filestream_delete(entry->backup_path);

                  backup_handle->num_auto_backups_to_remove--;
                  backup_handle->backup_index++;

                  /* Break here - only remove one file per
                   * iteration of the task */
                  break;
               }

               backup_handle->backup_index++;
            }

            /* Check whether all old backups have been
             * removed (also handle error conditions by
             * ensuring we quit if end of backup list is
             * reached...) */
            if ((backup_handle->num_auto_backups_to_remove < 1) ||
                (backup_handle->backup_index >= list_size))
               backup_handle->status = CORE_BACKUP_END;
         }
         break;
      case CORE_BACKUP_END:
         {
            char task_title[PATH_MAX_LENGTH];

            task_title[0] = '\0';

            /* Set final task title */
            task_free_title(task);

            if (backup_handle->success)
            {
               if (backup_handle->crc_match)
                  strlcpy(task_title, msg_hash_to_str(MSG_CORE_BACKUP_ALREADY_EXISTS),
                        sizeof(task_title));
               else
                  strlcpy(task_title, msg_hash_to_str(MSG_CORE_BACKUP_COMPLETE),
                        sizeof(task_title));
            }
            else
               strlcpy(task_title, msg_hash_to_str(MSG_CORE_BACKUP_FAILED),
                     sizeof(task_title));

            strlcat(task_title, backup_handle->core_name, sizeof(task_title));
            task_set_title(task, strdup(task_title));
         }
         /* fall-through */
      default:
         task_set_progress(task, 100);
         goto task_finished;
   }

   return;

task_finished:

   if (task)
      task_set_finished(task, true);

   free_core_backup_handle(backup_handle);
}

/* Note 1: If CRC is set to 0, CRC of core_path file will
 * be calculated automatically
 * Note 2: If core_display_name is set to NULL, display
 * name will be determined automatically
 * > core_display_name *must* be set to a non-empty
 *   string if task_push_core_backup() is *not* called
 *   on the main thread */
void *task_push_core_backup(
      const char *core_path, const char *core_display_name,
      uint32_t crc, enum core_backup_mode backup_mode,
      size_t auto_backup_history_size,
      const char *dir_core_assets, bool mute)
{
   task_finder_data_t find_data;
   const char *core_name               = NULL;
   retro_task_t *task                  = NULL;
   core_backup_handle_t *backup_handle = NULL;
   char task_title[PATH_MAX_LENGTH];

   task_title[0] = '\0';

   /* Sanity check */
   if (string_is_empty(core_path) ||
       !path_is_valid(core_path))
      goto error;

   /* Concurrent backup/restore tasks for the same core
    * are not allowed */
   find_data.func     = task_core_backup_finder;
   find_data.userdata = (void*)core_path;

   if (task_queue_find(&find_data))
      goto error;

   /* Get core name */
   if (!string_is_empty(core_display_name))
      core_name = core_display_name;
   else
   {
      core_info_t *core_info = NULL;

      /* If core is found, use display name */
      if (core_info_find(core_path, &core_info) &&
          core_info->display_name)
         core_name = core_info->display_name;
      else
      {
         /* If not, use core file name */
         core_name = path_basename(core_path);

         if (string_is_empty(core_name))
            goto error;
      }
   }

   /* Configure handle */
   backup_handle = (core_backup_handle_t*)calloc(1, sizeof(core_backup_handle_t));

   if (!backup_handle)
      goto error;

   backup_handle->dir_core_assets            = string_is_empty(dir_core_assets) ? NULL : strdup(dir_core_assets);
   backup_handle->core_path                  = strdup(core_path);
   backup_handle->core_name                  = strdup(core_name);
   backup_handle->backup_path                = NULL;
   backup_handle->backup_type                = CORE_BACKUP_TYPE_ARCHIVE;
   backup_handle->backup_mode                = backup_mode;
   backup_handle->auto_backup_history_size   = auto_backup_history_size;
   backup_handle->num_auto_backups_to_remove = 0;
   backup_handle->backup_index               = 0;
   backup_handle->core_file_size             = 0;
   backup_handle->backup_file_size           = 0;
   backup_handle->file_data_read             = 0;
   backup_handle->core_crc                   = crc;
   backup_handle->backup_crc                 = 0;
   backup_handle->crc_match                  = false;
   backup_handle->success                    = false;
   backup_handle->core_file                  = NULL;
   backup_handle->backup_file                = NULL;
   backup_handle->backup_list                = NULL;
   backup_handle->status                     = CORE_BACKUP_BEGIN;

   /* Create task */
   task = task_init();

   if (!task)
      goto error;

   /* Get initial task title */
   strlcpy(task_title, msg_hash_to_str(MSG_CORE_BACKUP_SCANNING_CORE),
         sizeof(task_title));
   strlcat(task_title, backup_handle->core_name, sizeof(task_title));

   /* Configure task */
   task->handler          = task_core_backup_handler;
   task->state            = backup_handle;
   task->mute             = mute;
   task->title            = strdup(task_title);
   task->alternative_look = true;
   task->progress         = 0;

   /* Push task */
   task_queue_push(task);

   return task;

error:

   /* Clean up task */
   if (task)
   {
      free(task);
      task = NULL;
   }

   /* Clean up handle */
   free_core_backup_handle(backup_handle);

   return NULL;
}

/****************/
/* Core Restore */
/****************/

static void cb_task_core_restore(
      retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   /* Reload core info files
    * > This must be done on the main thread */
   command_event(CMD_EVENT_CORE_INFO_INIT, NULL);
}

static void task_core_restore_handler(retro_task_t *task)
{
   core_backup_handle_t *backup_handle = NULL;

   if (!task)
      goto task_finished;

   backup_handle = (core_backup_handle_t*)task->state;

   if (!backup_handle)
      goto task_finished;

   if (task_get_cancelled(task))
      goto task_finished;

   switch (backup_handle->status)
   {
      case CORE_RESTORE_GET_CORE_CRC:
         {
            /* If core file already exists, get its current
             * CRC value */
            if (path_is_valid(backup_handle->core_path))
            {
               /* Open core file for reading */
               backup_handle->core_file = intfstream_open_file(
                     backup_handle->core_path, RETRO_VFS_FILE_ACCESS_READ,
                     RETRO_VFS_FILE_ACCESS_HINT_NONE);

               if (!backup_handle->core_file)
               {
                  RARCH_ERR("[core restore] Failed to open core file: %s\n",
                        backup_handle->core_path);
                  backup_handle->status = CORE_RESTORE_END;
                  break;
               }

               /* Get CRC value */
               if (!intfstream_get_crc(backup_handle->core_file,
                     &backup_handle->core_crc))
               {
                  RARCH_ERR("[core restore] Failed to determine CRC of core file: %s\n",
                        backup_handle->core_path);
                  backup_handle->status = CORE_RESTORE_END;
                  break;
               }

               /* Close core file */
               intfstream_close(backup_handle->core_file);
               free(backup_handle->core_file);
               backup_handle->core_file = NULL;
            }

            /* Go to next CRC gathering phase */
            backup_handle->status = CORE_RESTORE_GET_BACKUP_CRC;
         }
         break;
      case CORE_RESTORE_GET_BACKUP_CRC:
         /* Get CRC value of backup file */
         if (!core_backup_get_backup_crc(
                  backup_handle->backup_path, &backup_handle->backup_crc))
         {
            RARCH_ERR("[core restore] Failed to determine CRC of core backup file: %s\n",
                  backup_handle->backup_path);
            backup_handle->status = CORE_RESTORE_END;
            break;
         }

         /* Go to CRC comparison phase */
         backup_handle->status = CORE_RESTORE_CHECK_CRC;
         break;
      case CORE_RESTORE_CHECK_CRC:
         /* Check whether current core matches backup CRC */
         if (backup_handle->core_crc == backup_handle->backup_crc)
         {
            RARCH_LOG("[core restore] Selected backup core file is already installed: %s\n",
                  backup_handle->backup_path);

            backup_handle->crc_match = true;
            backup_handle->success   = true;
            backup_handle->status    = CORE_RESTORE_END;
            break;
         }

         /* Go to pre-iteration phase */
         backup_handle->status = CORE_RESTORE_PRE_ITERATE;
         break;
      case CORE_RESTORE_PRE_ITERATE:
         {
            char task_title[PATH_MAX_LENGTH];

            task_title[0] = '\0';

            /* Open backup file */
#if defined(HAVE_ZLIB)
            backup_handle->backup_file = intfstream_open_rzip_file(
                  backup_handle->backup_path, RETRO_VFS_FILE_ACCESS_READ);
#else
            backup_handle->backup_file = intfstream_open_file(
                  backup_handle->backup_path, RETRO_VFS_FILE_ACCESS_READ,
                  RETRO_VFS_FILE_ACCESS_HINT_NONE);
#endif
            if (!backup_handle->backup_file)
            {
               RARCH_ERR("[core restore] Failed to open core backup file: %s\n",
                     backup_handle->backup_path);
               backup_handle->status = CORE_RESTORE_END;
               break;
            }

            /* Get backup file size */
            backup_handle->backup_file_size = intfstream_get_size(backup_handle->backup_file);

            if (backup_handle->backup_file_size <= 0)
            {
               RARCH_ERR("[core restore] Core backup file is empty/invalid: %s\n",
                     backup_handle->backup_path);
               backup_handle->status = CORE_RESTORE_END;
               break;
            }

#if defined(ANDROID)
            /* If this is a Play Store build and the
             * core is currently installed via
             * play feature delivery, must delete
             * the existing core before attempting
             * to write any data */
            if (play_feature_delivery_enabled())
            {
               const char *core_filename = path_basename(
                     backup_handle->core_path);

               if (play_feature_delivery_core_installed(core_filename) &&
                   !play_feature_delivery_delete(core_filename))
               {
                  RARCH_ERR("[core restore] Failed to delete existing play feature delivery core: %s\n",
                        backup_handle->core_path);
                  backup_handle->status = CORE_RESTORE_END;
                  break;
               }
            }
#endif
            /* Open core file for writing */
            backup_handle->core_file = intfstream_open_file(
                  backup_handle->core_path, RETRO_VFS_FILE_ACCESS_WRITE,
                  RETRO_VFS_FILE_ACCESS_HINT_NONE);

            if (!backup_handle->core_file)
            {
               RARCH_ERR("[core restore] Failed to open core file: %s\n",
                     backup_handle->core_path);
               backup_handle->status = CORE_RESTORE_END;
               break;
            }

            /* Update task title */
            task_free_title(task);
            strlcpy(task_title, (backup_handle->backup_type == CORE_BACKUP_TYPE_ARCHIVE) ?
                        msg_hash_to_str(MSG_RESTORING_CORE) :
                              msg_hash_to_str(MSG_INSTALLING_CORE),
                  sizeof(task_title));
            strlcat(task_title, backup_handle->core_name, sizeof(task_title));
            task_set_title(task, strdup(task_title));

            /* Go to iteration phase */
            backup_handle->status = CORE_RESTORE_ITERATE;
         }
         break;
      case CORE_RESTORE_ITERATE:
         {
            int64_t data_read    = 0;
            int64_t data_written = 0;
            uint8_t buffer[CORE_BACKUP_CHUNK_SIZE];

            /* Read a single chunk from the backup file */
            data_read = intfstream_read(backup_handle->backup_file, buffer, sizeof(buffer));

            if (data_read < 0)
            {
               RARCH_ERR("[core restore] Failed to read from core backup file: %s\n",
                     backup_handle->backup_path);
               backup_handle->status = CORE_RESTORE_END;
               break;
            }

            backup_handle->file_data_read += data_read;

            /* Check whether we have reached the end of the file */
            if (data_read == 0)
            {
               /* Close backup file */
               intfstream_close(backup_handle->backup_file);
               free(backup_handle->backup_file);
               backup_handle->backup_file = NULL;

               /* Close core file */
               intfstream_flush(backup_handle->core_file);
               intfstream_close(backup_handle->core_file);
               free(backup_handle->core_file);
               backup_handle->core_file   = NULL;

               backup_handle->success = true;
               backup_handle->status  = CORE_RESTORE_END;
               break;
            }

            /* Write chunk to core file */
            data_written = intfstream_write(backup_handle->core_file, buffer, data_read);

            if (data_written != data_read)
            {
               RARCH_ERR("[core restore] Failed to write to core file: %s\n",
                     backup_handle->core_path);
               backup_handle->status = CORE_RESTORE_END;
               break;
            }

            /* Update progress display */
            task_set_progress(task,
                  (int8_t)((backup_handle->file_data_read * 100) / backup_handle->backup_file_size));
         }
         break;
      case CORE_RESTORE_END:
         {
            char task_title[PATH_MAX_LENGTH];

            task_title[0] = '\0';

            /* Set final task title */
            task_free_title(task);

            if (backup_handle->success)
            {
               if (backup_handle->crc_match)
                  strlcpy(task_title, (backup_handle->backup_type == CORE_BACKUP_TYPE_ARCHIVE) ?
                              msg_hash_to_str(MSG_CORE_RESTORATION_ALREADY_INSTALLED) :
                                    msg_hash_to_str(MSG_CORE_INSTALLATION_ALREADY_INSTALLED),
                        sizeof(task_title));
               else
                  strlcpy(task_title, (backup_handle->backup_type == CORE_BACKUP_TYPE_ARCHIVE) ?
                              msg_hash_to_str(MSG_CORE_RESTORATION_COMPLETE) :
                                    msg_hash_to_str(MSG_CORE_INSTALLATION_COMPLETE),
                        sizeof(task_title));
            }
            else
               strlcpy(task_title, (backup_handle->backup_type == CORE_BACKUP_TYPE_ARCHIVE) ?
                           msg_hash_to_str(MSG_CORE_RESTORATION_FAILED) :
                                 msg_hash_to_str(MSG_CORE_INSTALLATION_FAILED),
                     sizeof(task_title));

            strlcat(task_title, backup_handle->core_name, sizeof(task_title));
            task_set_title(task, strdup(task_title));
         }
         /* fall-through */
      default:
         task_set_progress(task, 100);
         goto task_finished;
   }

   return;

task_finished:

   if (task)
      task_set_finished(task, true);

   free_core_backup_handle(backup_handle);
}

bool task_push_core_restore(const char *backup_path, const char *dir_libretro,
      bool *core_loaded)
{
   task_finder_data_t find_data;
   enum core_backup_type backup_type;
   core_info_t *core_info              = NULL;
   const char *core_name               = NULL;
   retro_task_t *task                  = NULL;
   core_backup_handle_t *backup_handle = NULL;
   char core_path[PATH_MAX_LENGTH];
   char task_title[PATH_MAX_LENGTH];

   core_path[0]  = '\0';
   task_title[0] = '\0';

   /* Sanity check */
   if (string_is_empty(backup_path) ||
       !path_is_valid(backup_path) ||
       string_is_empty(dir_libretro) ||
       !core_loaded)
      goto error;

   /* Ensure core directory is valid */
   if (!path_is_directory(dir_libretro))
   {
      if (!path_mkdir(dir_libretro))
      {
         RARCH_ERR("[core restore] Failed to create core directory: %s\n", dir_libretro);
         goto error;
      }
   }

   /* Get core path */
   backup_type = core_backup_get_core_path(
         backup_path, dir_libretro, core_path, sizeof(core_path));

   if (backup_type == CORE_BACKUP_TYPE_INVALID)
   {
      const char *backup_filename = path_basename(backup_path);
      char msg[PATH_MAX_LENGTH];

      msg[0] = '\0';

      strlcpy(msg, msg_hash_to_str(MSG_CORE_RESTORATION_INVALID_CONTENT), sizeof(msg));
      strlcat(msg, backup_filename ? backup_filename : "", sizeof(msg));

      RARCH_ERR("[core restore] Invalid core file selected: %s\n", backup_path);
      runloop_msg_queue_push(msg, 1, 100, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      goto error;
   }

   /* Get core name
    * > If core is found, use display name */
   if (core_info_find(core_path, &core_info) &&
       core_info->display_name)
      core_name = core_info->display_name;
   else
   {
      /* > If not, use core file name */
      core_name = path_basename(core_path);

      if (string_is_empty(core_name))
         goto error;
   }

   /* Check whether core is locked */
   if (core_info_get_core_lock(core_path, true))
   {
      char msg[PATH_MAX_LENGTH];

      msg[0] = '\0';

      strlcpy(msg,
            (backup_type == CORE_BACKUP_TYPE_ARCHIVE) ?
                  msg_hash_to_str(MSG_CORE_RESTORATION_DISABLED) :
                        msg_hash_to_str(MSG_CORE_INSTALLATION_DISABLED),
            sizeof(msg));
      strlcat(msg, core_name, sizeof(msg));

      RARCH_ERR("[core restore] Restoration disabled - core is locked: %s\n", core_path);
      runloop_msg_queue_push(msg, 1, 100, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      goto error;
   }

   /* Concurrent backup/restore tasks for the same core
    * are not allowed */
   find_data.func     = task_core_backup_finder;
   find_data.userdata = (void*)core_path;

   if (task_queue_find(&find_data))
      goto error;

   /* Configure handle */
   backup_handle = (core_backup_handle_t*)calloc(1, sizeof(core_backup_handle_t));

   if (!backup_handle)
      goto error;

   backup_handle->dir_core_assets            = NULL;
   backup_handle->core_path                  = strdup(core_path);
   backup_handle->core_name                  = strdup(core_name);
   backup_handle->backup_path                = strdup(backup_path);
   backup_handle->backup_type                = backup_type;
   backup_handle->backup_mode                = CORE_BACKUP_MODE_MANUAL;
   backup_handle->auto_backup_history_size   = 0;
   backup_handle->num_auto_backups_to_remove = 0;
   backup_handle->backup_index               = 0;
   backup_handle->core_file_size             = 0;
   backup_handle->backup_file_size           = 0;
   backup_handle->file_data_read             = 0;
   backup_handle->core_crc                   = 0;
   backup_handle->backup_crc                 = 0;
   backup_handle->crc_match                  = false;
   backup_handle->success                    = false;
   backup_handle->core_file                  = NULL;
   backup_handle->backup_file                = NULL;
   backup_handle->backup_list                = NULL;
   backup_handle->status                     = CORE_RESTORE_GET_CORE_CRC;

   /* Create task */
   task = task_init();

   if (!task)
      goto error;

   /* Get initial task title */
   strlcpy(task_title, msg_hash_to_str(MSG_CORE_BACKUP_SCANNING_CORE),
         sizeof(task_title));
   strlcat(task_title, backup_handle->core_name, sizeof(task_title));

   /* Configure task */
   task->handler          = task_core_restore_handler;
   task->state            = backup_handle;
   task->title            = strdup(task_title);
   task->alternative_look = true;
   task->progress         = 0;
   task->callback         = cb_task_core_restore;

   /* If core to be restored is currently loaded, must
    * unload it before pushing the task */
   if (retroarch_ctl(RARCH_CTL_IS_CORE_LOADED, (void*)core_path))
   {
      command_event(CMD_EVENT_UNLOAD_CORE, NULL);
      *core_loaded = true;
   }
   else
      *core_loaded = false;

   /* Push task */
   task_queue_push(task);

   return true;

error:

   /* Clean up task */
   if (task)
   {
      free(task);
      task = NULL;
   }

   /* Clean up handle */
   free_core_backup_handle(backup_handle);

   return false;
}
