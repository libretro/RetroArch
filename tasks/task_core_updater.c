/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2019 - Brad Parker
 *  Copyright (C)      2019 - James Leaver
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
#include <net/net_http.h>
#include <streams/interface_stream.h>
#include <streams/file_stream.h>

#include "task_file_transfer.h"
#include "tasks_internal.h"

#include "../configuration.h"
#include "../retroarch.h"
#include "../command.h"
#include "../msg_hash.h"
#include "../verbosity.h"
#include "../core_updater_list.h"

#if defined(ANDROID)
#include "../file_path_special.h"
#include "../play_feature_delivery/play_feature_delivery.h"
#endif

#if defined(RARCH_INTERNAL) && defined(HAVE_MENU)
#include "../menu/menu_entries.h"
#endif

/* Get core updater list */
enum core_updater_list_status
{
   CORE_UPDATER_LIST_BEGIN = 0,
   CORE_UPDATER_LIST_WAIT,
   CORE_UPDATER_LIST_END
};

typedef struct core_updater_list_handle
{
   core_updater_list_t* core_list;
   retro_task_t *http_task;
   http_transfer_data_t *http_data;
   enum core_updater_list_status status;
   bool refresh_menu;
   bool http_task_finished;
   bool http_task_complete;
   bool http_task_success;
} core_updater_list_handle_t;

/* Download core */
enum core_updater_download_status
{
   CORE_UPDATER_DOWNLOAD_BEGIN = 0,
   CORE_UPDATER_DOWNLOAD_START_BACKUP,
   CORE_UPDATER_DOWNLOAD_WAIT_BACKUP,
   CORE_UPDATER_DOWNLOAD_START_TRANSFER,
   CORE_UPDATER_DOWNLOAD_WAIT_TRANSFER,
   CORE_UPDATER_DOWNLOAD_WAIT_DECOMPRESS,
   CORE_UPDATER_DOWNLOAD_END
};

typedef struct core_updater_download_handle
{
   char *path_dir_libretro;
   char *path_dir_core_assets;
   char *remote_filename;
   char *remote_core_path;
   char *local_download_path;
   char *local_core_path;
   char *display_name;
   retro_task_t *http_task;
   retro_task_t *decompress_task;
   retro_task_t *backup_task;
   size_t auto_backup_history_size;
   uint32_t local_crc;
   uint32_t remote_crc;
   enum core_updater_download_status status;
   bool crc_match;
   bool http_task_finished;
   bool http_task_complete;
   bool auto_backup;
   bool decompress_task_finished;
   bool decompress_task_complete;
   bool backup_enabled;
} core_updater_download_handle_t;

/* Update installed cores */
enum update_installed_cores_status
{
   UPDATE_INSTALLED_CORES_BEGIN = 0,
   UPDATE_INSTALLED_CORES_WAIT_LIST,
   UPDATE_INSTALLED_CORES_ITERATE,
   UPDATE_INSTALLED_CORES_UPDATE_CORE,
   UPDATE_INSTALLED_CORES_WAIT_DOWNLOAD,
   UPDATE_INSTALLED_CORES_END
};

typedef struct update_installed_cores_handle
{
   char *path_dir_libretro;
   char *path_dir_core_assets;
   core_updater_list_t* core_list;
   retro_task_t *list_task;
   retro_task_t *download_task;
   size_t auto_backup_history_size;
   size_t list_size;
   size_t list_index;
   size_t installed_index;
   unsigned num_updated;
   unsigned num_locked;
   enum update_installed_cores_status status;
   bool auto_backup;
} update_installed_cores_handle_t;

#if defined(ANDROID)
/* Play feature delivery core install */
enum play_feature_delivery_install_task_status
{
   PLAY_FEATURE_DELIVERY_INSTALL_BEGIN = 0,
   PLAY_FEATURE_DELIVERY_INSTALL_WAIT,
   PLAY_FEATURE_DELIVERY_INSTALL_END
};

typedef struct play_feature_delivery_install_handle
{
   char *core_filename;
   char *local_core_path;
   char *backup_core_path;
   char *display_name;
   enum play_feature_delivery_install_task_status status;
   bool success;
   bool core_already_installed;
} play_feature_delivery_install_handle_t;

/* Play feature delivery switch installed cores */
enum play_feature_delivery_switch_cores_task_status
{
   PLAY_FEATURE_DELIVERY_SWITCH_CORES_BEGIN = 0,
   PLAY_FEATURE_DELIVERY_SWITCH_CORES_ITERATE,
   PLAY_FEATURE_DELIVERY_SWITCH_CORES_INSTALL_CORE,
   PLAY_FEATURE_DELIVERY_SWITCH_CORES_WAIT_INSTALL,
   PLAY_FEATURE_DELIVERY_SWITCH_CORES_END
};

typedef struct play_feature_delivery_switch_cores_handle
{
   char *path_dir_libretro;
   char *path_libretro_info;
   char *error_msg;
   core_updater_list_t* core_list;
   retro_task_t *install_task;
   size_t list_size;
   size_t list_index;
   size_t installed_index;
   enum play_feature_delivery_switch_cores_task_status status;
} play_feature_delivery_switch_cores_handle_t;
#endif

/*********************/
/* Utility functions */
/*********************/

/* Returns CRC32 of specified core file */
static uint32_t task_core_updater_get_core_crc(const char *core_path)
{
   if (string_is_empty(core_path))
      return 0;

   if (path_is_valid(core_path))
   {
      /* Open core file */
      intfstream_t *core_file = intfstream_open_file(
            core_path, RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);

      if (core_file)
      {
         uint32_t crc = 0;

         /* Get CRC value */
         bool success = intfstream_get_crc(core_file, &crc);

         /* Close core file */
         intfstream_close(core_file);
         free(core_file);
         core_file = NULL;

         if (success)
            return crc;
      }
   }

   return 0;
}

/*************************/
/* Get core updater list */
/*************************/

static void cb_http_task_core_updater_get_list(
      retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   http_transfer_data_t *data              = (http_transfer_data_t*)task_data;
   file_transfer_t *transf                 = (file_transfer_t*)user_data;
   core_updater_list_handle_t *list_handle = NULL;
   bool success                            = data && string_is_empty(err);

   if (!transf)
      goto finish;

   list_handle = (core_updater_list_handle_t*)transf->user_data;

   if (!list_handle)
      goto finish;

   task_set_data(task, NULL); /* going to pass ownership to list_handle */

   list_handle->http_data          = data;
   list_handle->http_task_complete = true;
   list_handle->http_task_success  = success;


finish:

   /* Log any error messages */
   if (!success)
      RARCH_ERR("[core updater] Download of core list '%s' failed: %s\n",
            (transf ? transf->path: "unknown"),
            (err ? err : "unknown"));

   if (transf)
      free(transf);
}

static void free_core_updater_list_handle(
      core_updater_list_handle_t *list_handle)
{
   if (!list_handle)
      return;

   if (list_handle->http_data)
   {
      /* since we took onwership, we have to destroy it ourself */
      if (list_handle->http_data->data)
         free(list_handle->http_data->data);

      free(list_handle->http_data);
   }

   free(list_handle);
   list_handle = NULL;
}

static void task_core_updater_get_list_handler(retro_task_t *task)
{
   core_updater_list_handle_t *list_handle = NULL;

   if (!task)
      goto task_finished;

   list_handle = (core_updater_list_handle_t*)task->state;

   if (!list_handle)
      goto task_finished;

   if (task_get_cancelled(task))
      goto task_finished;

   switch (list_handle->status)
   {
      case CORE_UPDATER_LIST_BEGIN:
         {
            settings_t *settings    = config_get_ptr();
            file_transfer_t *transf = NULL;
            char *tmp_url           = NULL;
            char buildbot_url[PATH_MAX_LENGTH];
            const char *net_buildbot_url = 
               settings->paths.network_buildbot_url;

            buildbot_url[0] = '\0';

            /* Reset core updater list */
            core_updater_list_reset(list_handle->core_list);

            /* Get core listing URL */
            if (!settings)
               goto task_finished;

            if (string_is_empty(net_buildbot_url))
               goto task_finished;

            fill_pathname_join(
                  buildbot_url,
                  net_buildbot_url,
                  ".index-extended",
                  sizeof(buildbot_url));

            tmp_url = strdup(buildbot_url);
            buildbot_url[0] = '\0';
            net_http_urlencode_full(
                  buildbot_url, tmp_url, sizeof(buildbot_url));
            if (tmp_url)
               free(tmp_url);

            if (string_is_empty(buildbot_url))
               goto task_finished;

            /* Configure file transfer object */
            transf = (file_transfer_t*)calloc(1, sizeof(file_transfer_t));

            if (!transf)
               goto task_finished;

            /* > Seems to be required - not sure why the
             *   underlying code is implemented like this... */
            strlcpy(transf->path, buildbot_url, sizeof(transf->path));

            transf->user_data = (void*)list_handle;

            /* Push HTTP transfer task */
            list_handle->http_task = (retro_task_t*)task_push_http_transfer_file(
                  buildbot_url, true, NULL,
                  cb_http_task_core_updater_get_list, transf);

            /* Start waiting for HTTP transfer to complete */
            list_handle->status = CORE_UPDATER_LIST_WAIT;
         }
         break;
      case CORE_UPDATER_LIST_WAIT:
         {
            /* If HTTP task is NULL, then it either finished
             * or an error occurred - in either case,
             * just move on to the next state */
            if (!list_handle->http_task)
               list_handle->http_task_complete = true;
            /* Otherwise, check if HTTP task is still running */
            else if (!list_handle->http_task_finished)
            {
               list_handle->http_task_finished =
                     task_get_finished(list_handle->http_task);

               /* If HTTP task is running, copy current
                * progress value to *this* task */
               if (!list_handle->http_task_finished)
                  task_set_progress(
                     task, task_get_progress(list_handle->http_task));
            }

            /* Wait for task_push_http_transfer_file()
             * callback to trigger */
            if (list_handle->http_task_complete)
               list_handle->status = CORE_UPDATER_LIST_END;
         }
         break;
      case CORE_UPDATER_LIST_END:
         {
            settings_t *settings    = config_get_ptr();

            /* Check whether HTTP task was successful */
            if (list_handle->http_task_success)
            {
               /* Parse HTTP transfer data */
               if (list_handle->http_data)
                  core_updater_list_parse_network_data(
                        list_handle->core_list,
                        settings->paths.directory_libretro,
                        settings->paths.path_libretro_info,
                        settings->paths.network_buildbot_url,
                        list_handle->http_data->data,
                        list_handle->http_data->len);
            }
            else
            {
               /* Notify user of error via task title */
               task_free_title(task);
               task_set_title(task, strdup(msg_hash_to_str(MSG_CORE_LIST_FAILED)));
            }

            /* Enable menu refresh, if required */
#if defined(RARCH_INTERNAL) && defined(HAVE_MENU)
            if (list_handle->refresh_menu)
               menu_entries_ctl(
                     MENU_ENTRIES_CTL_UNSET_REFRESH,
                     &list_handle->refresh_menu);
#endif
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

   free_core_updater_list_handle(list_handle);
}

static bool task_core_updater_get_list_finder(retro_task_t *task, void *user_data)
{
   core_updater_list_handle_t *list_handle = NULL;

   if (!task || !user_data)
      return false;

   if (task->handler != task_core_updater_get_list_handler)
      return false;

   list_handle = (core_updater_list_handle_t*)task->state;
   if (!list_handle)
      return false;

   return ((uintptr_t)user_data == (uintptr_t)list_handle->core_list);
}

void *task_push_get_core_updater_list(
      core_updater_list_t* core_list, bool mute, bool refresh_menu)
{
   task_finder_data_t find_data;
   retro_task_t *task                      = NULL;
   core_updater_list_handle_t *list_handle = (core_updater_list_handle_t*)
         calloc(1, sizeof(core_updater_list_handle_t));

#if defined(ANDROID)
   /* Regular core updater is disabled in
    * Play Store builds */
   if (play_feature_delivery_enabled())
      goto error;
#endif

   /* Sanity check */
   if (!core_list || !list_handle)
      goto error;

   /* Configure handle */
   list_handle->core_list          = core_list;
   list_handle->refresh_menu       = refresh_menu;
   list_handle->http_task          = NULL;
   list_handle->http_task_finished = false;
   list_handle->http_task_complete = false;
   list_handle->http_task_success  = false;
   list_handle->http_data          = NULL;
   list_handle->status             = CORE_UPDATER_LIST_BEGIN;

   /* Concurrent downloads of the buildbot core listing
    * to the same core_updater_list_t object are not
    * allowed */
   find_data.func     = task_core_updater_get_list_finder;
   find_data.userdata = (void*)core_list;

   if (task_queue_find(&find_data))
      goto error;

   /* Create task */
   task = task_init();

   if (!task)
      goto error;

   /* Configure task */
   task->handler          = task_core_updater_get_list_handler;
   task->state            = list_handle;
   task->mute             = mute;
   task->title            = strdup(msg_hash_to_str(MSG_FETCHING_CORE_LIST));
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
   free_core_updater_list_handle(list_handle);

   return NULL;
}

/*****************/
/* Download core */
/*****************/

static void cb_task_core_updater_download(
      retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   /* Reload core info files
    * > This must be done on the main thread */
   command_event(CMD_EVENT_CORE_INFO_INIT, NULL);
}

static void cb_decompress_task_core_updater_download(
      retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   decompress_task_data_t *decompress_data         =
         (decompress_task_data_t*)task_data;
   core_updater_download_handle_t *download_handle =
         (core_updater_download_handle_t*)user_data;

   /* Signal that decompression task is complete */
   if (download_handle)
      download_handle->decompress_task_complete = true;

   /* Remove original archive file */
   if (decompress_data)
   {
      if (!string_is_empty(decompress_data->source_file))
         if (path_is_valid(decompress_data->source_file))
            filestream_delete(decompress_data->source_file);

      if (decompress_data->source_file)
         free(decompress_data->source_file);

      free(decompress_data);
   }

   /* Log any error messages */
   if (!string_is_empty(err))
      RARCH_ERR("[core updater] %s", err);
}

void cb_http_task_core_updater_download(
      retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   http_transfer_data_t *data                      = (http_transfer_data_t*)task_data;
   file_transfer_t *transf                         = (file_transfer_t*)user_data;
   core_updater_download_handle_t *download_handle = NULL;
   char output_dir[PATH_MAX_LENGTH];

   output_dir[0] = '\0';

   if (!data || !transf)
      goto finish;

   if (!data->data || string_is_empty(transf->path))
      goto finish;

   download_handle = (core_updater_download_handle_t*)transf->user_data;

   if (!download_handle)
      goto finish;

   /* Update download_handle task status
    * NOTE: We set decompress_task_complete = true
    * here to prevent any lock-ups in the event
    * of errors (or lack of decompression support).
    * decompress_task_complete will be set false
    * if/when we actually call task_push_decompress() */
   download_handle->http_task_complete       = true;
   download_handle->decompress_task_complete = true;

   /* Create output directory, if required */
   strlcpy(output_dir, transf->path, sizeof(output_dir));
   path_basedir_wrapper(output_dir);

   if (!path_mkdir(output_dir))
   {
      err = msg_hash_to_str(MSG_FAILED_TO_CREATE_THE_DIRECTORY);
      goto finish;
   }

#ifdef HAVE_COMPRESSION
   /* If core file is an archive, make sure it is
    * not being decompressed already (by another task) */
   if (path_is_compressed_file(transf->path))
   {
      if (task_check_decompress(transf->path))
      {
         err = msg_hash_to_str(MSG_DECOMPRESSION_ALREADY_IN_PROGRESS);
         goto finish;
      }
   }
#endif

   /* Write core file to disk */
   if (!filestream_write_file(transf->path, data->data, data->len))
   {
      err = "Write failed.";
      goto finish;
   }

#if defined(HAVE_COMPRESSION) && defined(HAVE_ZLIB)
   /* Decompress core file, if required
    * NOTE: If core is compressed and platform
    * doesn't have compression support, then this
    * whole thing falls apart...
    * We assume that the build process is configured
    * in such a way that this cannot happen... */
   if (path_is_compressed_file(transf->path))
   {
      download_handle->decompress_task = (retro_task_t*)task_push_decompress(
            transf->path, output_dir,
            NULL, NULL, NULL,
            cb_decompress_task_core_updater_download,
            (void*)download_handle,
            NULL, true);

      if (!download_handle->decompress_task)
      {
         err = msg_hash_to_str(MSG_DECOMPRESSION_FAILED);
         goto finish;
      }

      download_handle->decompress_task_complete = false;
   }
#endif

finish:

   /* Log any error messages */
   if (!string_is_empty(err))
      RARCH_ERR("[core updater] Download of '%s' failed: %s\n",
            (transf ? transf->path: "unknown"), err);

   if (transf)
      free(transf);
}

static void free_core_updater_download_handle(core_updater_download_handle_t *download_handle)
{
   if (!download_handle)
      return;

   if (download_handle->path_dir_libretro)
      free(download_handle->path_dir_libretro);

   if (download_handle->path_dir_core_assets)
      free(download_handle->path_dir_core_assets);

   if (download_handle->remote_filename)
      free(download_handle->remote_filename);

   if (download_handle->remote_core_path)
      free(download_handle->remote_core_path);

   if (download_handle->local_download_path)
      free(download_handle->local_download_path);

   if (download_handle->local_core_path)
      free(download_handle->local_core_path);

   if (download_handle->display_name)
      free(download_handle->display_name);

   free(download_handle);
   download_handle = NULL;
}

static void task_core_updater_download_handler(retro_task_t *task)
{
   core_updater_download_handle_t *download_handle = NULL;

   if (!task)
      goto task_finished;

   download_handle = (core_updater_download_handle_t*)task->state;

   if (!download_handle)
      goto task_finished;

   if (task_get_cancelled(task))
      goto task_finished;

   switch (download_handle->status)
   {
      case CORE_UPDATER_DOWNLOAD_BEGIN:
         {
            /* Get CRC of existing core, if required */
            if (download_handle->local_crc == 0)
               download_handle->local_crc = task_core_updater_get_core_crc(
                     download_handle->local_core_path);

            /* Check whether existing core and remote core
             * have the same CRC */
            download_handle->crc_match = (download_handle->local_crc != 0) &&
                  (download_handle->local_crc == download_handle->remote_crc);

            /* If CRC matches, end task immediately */
            if (download_handle->crc_match)
            {
               download_handle->status = CORE_UPDATER_DOWNLOAD_END;
               break;
            }

            /* If automatic backups are enabled and core is
             * already installed, trigger a backup - otherwise,
             * initialise download */
            download_handle->backup_enabled = download_handle->auto_backup &&
                  path_is_valid(download_handle->local_core_path);

            download_handle->status = download_handle->backup_enabled ?
                  CORE_UPDATER_DOWNLOAD_START_BACKUP :
                        CORE_UPDATER_DOWNLOAD_START_TRANSFER;
         }
         break;
      case CORE_UPDATER_DOWNLOAD_START_BACKUP:
         {
            /* Request core backup */
            download_handle->backup_task = (retro_task_t*)task_push_core_backup(
                  download_handle->local_core_path,
                  download_handle->display_name,
                  download_handle->local_crc, CORE_BACKUP_MODE_AUTO,
                  download_handle->auto_backup_history_size,
                  download_handle->path_dir_core_assets, true);

            if (download_handle->backup_task)
            {
               char task_title[PATH_MAX_LENGTH];

               task_title[0] = '\0';

               /* Update task title */
               task_free_title(task);

               strlcpy(
                     task_title, msg_hash_to_str(MSG_BACKING_UP_CORE),
                     sizeof(task_title));
               strlcat(task_title, download_handle->display_name, sizeof(task_title));

               task_set_title(task, strdup(task_title));

               /* Start waiting for backup to complete */
               download_handle->status = CORE_UPDATER_DOWNLOAD_WAIT_BACKUP;
            }
            else
            {
               /* This cannot realistically happen...
                * > If it does, just log an error and initialise
                *   download */
               RARCH_ERR("[core updater] Failed to backup core: %s\n",
                     download_handle->local_core_path);
               download_handle->backup_enabled = false;
               download_handle->status         = CORE_UPDATER_DOWNLOAD_START_TRANSFER;
            }
         }
         break;
      case CORE_UPDATER_DOWNLOAD_WAIT_BACKUP:
         {
            bool backup_complete = false;

            /* > If task is running, check 'is finished'
             *   status
             * > If task is NULL, then it is finished
             *   by definition */
            if (download_handle->backup_task)
            {
               backup_complete = task_get_finished(download_handle->backup_task);

               /* If backup task is running, copy current
                * progress value to *this* task */
               if (!backup_complete)
               {
                  /* Backup accounts for first third of
                   * task progress */
                  int8_t progress = task_get_progress(download_handle->backup_task);

                  task_set_progress(task, (int8_t)(((float)progress * (1.0f / 3.0f)) + 0.5f));
               }
            }
            else
               backup_complete = true;

            /* If backup is complete, initialise download */
            if (backup_complete)
            {
               download_handle->backup_task = NULL;
               download_handle->status      = CORE_UPDATER_DOWNLOAD_START_TRANSFER;
            }
         }
         break;
      case CORE_UPDATER_DOWNLOAD_START_TRANSFER:
         {
            file_transfer_t *transf = NULL;
            char task_title[PATH_MAX_LENGTH];

            task_title[0] = '\0';

            /* Configure file transfer object */
            transf = (file_transfer_t*)calloc(1, sizeof(file_transfer_t));

            if (!transf)
               goto task_finished;

            strlcpy(
                  transf->path, download_handle->local_download_path,
                  sizeof(transf->path));

            transf->user_data = (void*)download_handle;

            /* Push HTTP transfer task */
            download_handle->http_task = (retro_task_t*)task_push_http_transfer_file(
                  download_handle->remote_core_path, true, NULL,
                  cb_http_task_core_updater_download, transf);

            /* Update task title */
            task_free_title(task);

            strlcpy(
                  task_title, msg_hash_to_str(MSG_DOWNLOADING_CORE),
                  sizeof(task_title));
            strlcat(task_title, download_handle->display_name, sizeof(task_title));

            task_set_title(task, strdup(task_title));

            /* Start waiting for HTTP transfer to complete */
            download_handle->status = CORE_UPDATER_DOWNLOAD_WAIT_TRANSFER;
         }
         break;
      case CORE_UPDATER_DOWNLOAD_WAIT_TRANSFER:
         {
            /* If HTTP task is NULL, then it either finished
             * or an error occurred - in either case,
             * just move on to the next state */
            if (!download_handle->http_task)
               download_handle->http_task_complete = true;
            /* Otherwise, check if HTTP task is still running */
            else if (!download_handle->http_task_finished)
            {
               download_handle->http_task_finished =
                     task_get_finished(download_handle->http_task);

               /* If HTTP task is running, copy current
                * progress value to *this* task */
               if (!download_handle->http_task_finished)
               {
                  /* > If backups are enabled, download accounts
                   *   for second third of task progress
                   * > Otherwise, download accounts for first half
                   *   of task progress */
                  int8_t progress = task_get_progress(download_handle->http_task);

                  if (download_handle->backup_enabled)
                     progress = (int8_t)(((float)progress * (1.0f / 3.0f)) + (100.0f / 3.0f) + 0.5f);
                  else
                     progress = progress >> 1;

                  task_set_progress(task, progress);
               }
            }

            /* Wait for task_push_http_transfer_file()
             * callback to trigger */
            if (download_handle->http_task_complete)
            {
               char task_title[PATH_MAX_LENGTH];

               task_title[0] = '\0';

               /* Update task title */
               task_free_title(task);

               strlcpy(
                     task_title, msg_hash_to_str(MSG_EXTRACTING_CORE),
                     sizeof(task_title));
               strlcat(task_title, download_handle->display_name, sizeof(task_title));

               task_set_title(task, strdup(task_title));

               /* Start waiting for file to be extracted */
               download_handle->status = CORE_UPDATER_DOWNLOAD_WAIT_DECOMPRESS;
            }
         }
         break;
      case CORE_UPDATER_DOWNLOAD_WAIT_DECOMPRESS:
         {
            /* If decompression task is NULL, then it either
             * finished or an error occurred - in either case,
             * just move on to the next state */
            if (!download_handle->decompress_task)
               download_handle->decompress_task_complete = true;
            /* Otherwise, check if decompression task is still
             * running */
            else if (!download_handle->decompress_task_finished)
            {
               download_handle->decompress_task_finished =
                     task_get_finished(download_handle->decompress_task);

               /* If decompression task is running, copy
                * current progress value to *this* task */
               if (!download_handle->decompress_task_finished)
               {
                  /* > If backups are enabled, decompression accounts
                   *   for last third of task progress
                   * > Otherwise, decompression accounts for second
                   *   half of task progress */
                  int8_t progress = task_get_progress(download_handle->decompress_task);

                  if (download_handle->backup_enabled)
                     progress = (int8_t)(((float)progress * (1.0f / 3.0f)) + (200.0f / 3.0f) + 0.5f);
                  else
                     progress = 50 + (progress >> 1);

                  task_set_progress(task, progress);
               }
            }

            /* Wait for task_push_decompress()
             * callback to trigger */
            if (download_handle->decompress_task_complete)
               download_handle->status = CORE_UPDATER_DOWNLOAD_END;
         }
         break;
      case CORE_UPDATER_DOWNLOAD_END:
         {
            char task_title[PATH_MAX_LENGTH];

            task_title[0] = '\0';

            /* Set final task title */
            task_free_title(task);

            strlcpy(
                  task_title,
                  download_handle->crc_match ?
                        msg_hash_to_str(MSG_LATEST_CORE_INSTALLED) : msg_hash_to_str(MSG_CORE_INSTALLED),
                  sizeof(task_title));
            strlcat(task_title, download_handle->display_name, sizeof(task_title));

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

   free_core_updater_download_handle(download_handle);
}

static bool task_core_updater_download_finder(retro_task_t *task, void *user_data)
{
   core_updater_download_handle_t *download_handle = NULL;

   if (!task || !user_data)
      return false;

   if (task->handler != task_core_updater_download_handler)
      return false;

   download_handle = (core_updater_download_handle_t*)task->state;
   if (!download_handle)
      return false;

   return string_is_equal((const char*)user_data, download_handle->remote_filename);
}

void *task_push_core_updater_download(
      core_updater_list_t* core_list,
      const char *filename, uint32_t crc, bool mute,
      bool auto_backup, size_t auto_backup_history_size,
      const char *path_dir_libretro,
      const char *path_dir_core_assets)
{
   task_finder_data_t find_data;
   char task_title[PATH_MAX_LENGTH];
   char local_download_path[PATH_MAX_LENGTH];
   const core_updater_list_entry_t *list_entry     = NULL;
   retro_task_t *task                              = NULL;
   core_updater_download_handle_t *download_handle = (core_updater_download_handle_t*)
         calloc(1, sizeof(core_updater_download_handle_t));

   task_title[0]          = '\0';
   local_download_path[0] = '\0';

#if defined(ANDROID)
   /* Regular core updater is disabled in
    * Play Store builds */
   if (play_feature_delivery_enabled())
      goto error;
#endif

   /* Sanity check */
   if (!core_list ||
       string_is_empty(filename) ||
       !download_handle)
      goto error;

   /* Get core updater list entry */
   if (!core_updater_list_get_filename(
         core_list, filename, &list_entry))
      goto error;

   if (string_is_empty(list_entry->remote_core_path) ||
       string_is_empty(list_entry->local_core_path) ||
       string_is_empty(list_entry->display_name))
      goto error;

   /* Check whether core is locked
    * > Have to set validate_path to 'false' here,
    *   since this may not run on the main thread
    * > Validation is not required anyway, since core
    *   updater list provides 'sane' core paths */
   if (core_info_get_core_lock(list_entry->local_core_path, false))
   {
      RARCH_ERR("[core updater] Update disabled - core is locked: %s\n",
            list_entry->local_core_path);

      /* If task is not muted, generate notification */
      if (!mute)
      {
         char msg[PATH_MAX_LENGTH];

         msg[0] = '\0';

         strlcpy(msg, msg_hash_to_str(MSG_CORE_UPDATE_DISABLED), sizeof(msg));
         strlcat(msg, list_entry->display_name, sizeof(msg));

         runloop_msg_queue_push(msg, 1, 100, true,
               NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }

      goto error;
   }

   /* Get local file download path */
   if (string_is_empty(path_dir_libretro))
      goto error;

   fill_pathname_join(
         local_download_path,
         path_dir_libretro,
         list_entry->remote_filename,
         sizeof(local_download_path));

   /* Configure handle */
   download_handle->auto_backup              = auto_backup;
   download_handle->auto_backup_history_size = auto_backup_history_size;
   download_handle->path_dir_libretro        = strdup(path_dir_libretro);
   download_handle->path_dir_core_assets     = string_is_empty(path_dir_core_assets) ? NULL : strdup(path_dir_core_assets);
   download_handle->remote_filename          = strdup(list_entry->remote_filename);
   download_handle->remote_core_path         = strdup(list_entry->remote_core_path);
   download_handle->local_download_path      = strdup(local_download_path);
   download_handle->local_core_path          = strdup(list_entry->local_core_path);
   download_handle->display_name             = strdup(list_entry->display_name);
   download_handle->local_crc                = crc;
   download_handle->remote_crc               = list_entry->crc;
   download_handle->crc_match                = false;
   download_handle->http_task                = NULL;
   download_handle->http_task_finished       = false;
   download_handle->http_task_complete       = false;
   download_handle->decompress_task          = NULL;
   download_handle->decompress_task_finished = false;
   download_handle->decompress_task_complete = false;
   download_handle->backup_enabled           = false;
   download_handle->backup_task              = NULL;
   download_handle->status                   = CORE_UPDATER_DOWNLOAD_BEGIN;

   /* Concurrent downloads of the same file are not allowed */
   find_data.func     = task_core_updater_download_finder;
   find_data.userdata = (void*)download_handle->remote_filename;

   if (task_queue_find(&find_data))
      goto error;

   /* Create task */
   task = task_init();

   if (!task)
      goto error;

   /* Configure task */
   strlcpy(
         task_title, msg_hash_to_str(MSG_UPDATING_CORE),
         sizeof(task_title));
   strlcat(task_title, download_handle->display_name, sizeof(task_title));

   task->handler          = task_core_updater_download_handler;
   task->state            = download_handle;
   task->mute             = mute;
   task->title            = strdup(task_title);
   task->alternative_look = true;
   task->progress         = 0;
   task->callback         = cb_task_core_updater_download;

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
   free_core_updater_download_handle(download_handle);

   return NULL;
}

/**************************/
/* Update installed cores */
/**************************/

static void free_update_installed_cores_handle(
      update_installed_cores_handle_t *update_installed_handle)
{
   if (!update_installed_handle)
      return;

   if (update_installed_handle->path_dir_libretro)
      free(update_installed_handle->path_dir_libretro);

   if (update_installed_handle->path_dir_core_assets)
      free(update_installed_handle->path_dir_core_assets);

   core_updater_list_free(update_installed_handle->core_list);

   free(update_installed_handle);
   update_installed_handle = NULL;
}

static void task_update_installed_cores_handler(retro_task_t *task)
{
   update_installed_cores_handle_t *update_installed_handle = NULL;

   if (!task)
      goto task_finished;

   update_installed_handle = (update_installed_cores_handle_t*)task->state;

   if (!update_installed_handle)
      goto task_finished;

   if (task_get_cancelled(task))
      goto task_finished;

   switch (update_installed_handle->status)
   {
      case UPDATE_INSTALLED_CORES_BEGIN:
         /* Request buildbot core list */
         update_installed_handle->list_task = (retro_task_t*)
            task_push_get_core_updater_list(
                  update_installed_handle->core_list,
                  true, false);

         /* If push failed, go to end
          * (error will message will be displayed when
          * final task title is set) */
         if (!update_installed_handle->list_task)
            update_installed_handle->status = UPDATE_INSTALLED_CORES_END;
         else
            update_installed_handle->status = UPDATE_INSTALLED_CORES_WAIT_LIST;
         break;
      case UPDATE_INSTALLED_CORES_WAIT_LIST:
         {
            bool list_available = false;

            /* > If task is running, check 'is finished'
             *   status
             * > If task is NULL, then it is finished
             *   by definition */
            if (update_installed_handle->list_task)
               list_available = task_get_finished(update_installed_handle->list_task);
            else
               list_available = true;

            /* If list is available, make sure it isn't empty
             * (error will message will be displayed when
             * final task title is set) */
            if (list_available)
            {
               update_installed_handle->list_size =
                     core_updater_list_size(update_installed_handle->core_list);

               if (update_installed_handle->list_size < 1)
                  update_installed_handle->status = UPDATE_INSTALLED_CORES_END;
               else
                  update_installed_handle->status = UPDATE_INSTALLED_CORES_ITERATE;
            }
         }
         break;
      case UPDATE_INSTALLED_CORES_ITERATE:
         {
            const core_updater_list_entry_t *list_entry = NULL;
            bool core_installed                         = false;

            /* Check whether we have reached the end
             * of the list */
            if (update_installed_handle->list_index >= update_installed_handle->list_size)
            {
               update_installed_handle->status = UPDATE_INSTALLED_CORES_END;
               break;
            }

            /* Check whether current core is installed */
            if (core_updater_list_get_index(
                  update_installed_handle->core_list,
                  update_installed_handle->list_index,
                  &list_entry))
            {
               if (path_is_valid(list_entry->local_core_path))
               {
                  core_installed                           = true;
                  update_installed_handle->installed_index =
                        update_installed_handle->list_index;
                  update_installed_handle->status          =
                        UPDATE_INSTALLED_CORES_UPDATE_CORE;
               }
            }

            /* Update progress display */
            task_free_title(task);

            if (core_installed)
            {
               char task_title[PATH_MAX_LENGTH];

               task_title[0] = '\0';

               strlcpy(
                     task_title, msg_hash_to_str(MSG_CHECKING_CORE),
                     sizeof(task_title));
               strlcat(task_title, list_entry->display_name, sizeof(task_title));

               task_set_title(task, strdup(task_title));
            }
            else
               task_set_title(task, strdup(msg_hash_to_str(MSG_SCANNING_CORES)));

            task_set_progress(task,
                  (update_installed_handle->list_index * 100) /
                        update_installed_handle->list_size);

            /* Increment list index */
            update_installed_handle->list_index++;
         }
         break;
      case UPDATE_INSTALLED_CORES_UPDATE_CORE:
         {
            const core_updater_list_entry_t *list_entry = NULL;
            uint32_t local_crc;

            /* Get list entry
             * > In the event of an error, just return
             *   to UPDATE_INSTALLED_CORES_ITERATE state */
            if (!core_updater_list_get_index(
                  update_installed_handle->core_list,
                  update_installed_handle->installed_index,
                  &list_entry))
            {
               update_installed_handle->status = UPDATE_INSTALLED_CORES_ITERATE;
               break;
            }

            /* Check whether core is locked
             * > Have to set validate_path to 'false' here,
             *   since this does not run on the main thread
             * > Validation is not required anyway, since core
             *   updater list provides 'sane' core paths */
            if (core_info_get_core_lock(list_entry->local_core_path, false))
            {
               RARCH_LOG("[core updater] Skipping locked core: %s\n",
                     list_entry->display_name);

               /* Core update is disabled
                * > Just increment 'locked cores' counter and
                *   return to UPDATE_INSTALLED_CORES_ITERATE state */
               update_installed_handle->num_locked++;
               update_installed_handle->status = UPDATE_INSTALLED_CORES_ITERATE;
               break;
            }

            /* Get CRC of existing core */
            local_crc = task_core_updater_get_core_crc(
                  list_entry->local_core_path);

            /* Check whether existing core and remote core
             * have the same CRC
             * > If CRC matches, then core is already the most
             *   recent version - just return to
             *   UPDATE_INSTALLED_CORES_ITERATE state */
            if ((local_crc != 0) && (local_crc == list_entry->crc))
            {
               update_installed_handle->status = UPDATE_INSTALLED_CORES_ITERATE;
               break;
            }

            /* Existing core is not the most recent version
             * > Request download */
            update_installed_handle->download_task = (retro_task_t*)
                  task_push_core_updater_download(
                        update_installed_handle->core_list,
                        list_entry->remote_filename,
                        local_crc, true,
                        update_installed_handle->auto_backup,
                        update_installed_handle->auto_backup_history_size,
                        update_installed_handle->path_dir_libretro,
                        update_installed_handle->path_dir_core_assets);

            /* Again, if an error occurred, just return to
             * UPDATE_INSTALLED_CORES_ITERATE state */
            if (!update_installed_handle->download_task)
               update_installed_handle->status = UPDATE_INSTALLED_CORES_ITERATE;
            else
            {
               char task_title[PATH_MAX_LENGTH];

               task_title[0] = '\0';

               /* Update task title */
               task_free_title(task);

               strlcpy(
                     task_title, msg_hash_to_str(MSG_UPDATING_CORE),
                     sizeof(task_title));
               strlcat(task_title, list_entry->display_name, sizeof(task_title));

               task_set_title(task, strdup(task_title));

               /* Increment 'updated cores' counter */
               update_installed_handle->num_updated++;

               /* Wait for download to complete */
               update_installed_handle->status = UPDATE_INSTALLED_CORES_WAIT_DOWNLOAD;
            }
         }
         break;
      case UPDATE_INSTALLED_CORES_WAIT_DOWNLOAD:
         {
            bool download_complete = false;

            /* > If task is running, check 'is finished'
             *   status
             * > If task is NULL, then it is finished
             *   by definition */
            if (update_installed_handle->download_task)
               download_complete = task_get_finished(update_installed_handle->download_task);
            else
               download_complete = true;

            /* If download is complete, return to
             * UPDATE_INSTALLED_CORES_ITERATE state */
            if (download_complete)
            {
               update_installed_handle->download_task = NULL;
               update_installed_handle->status        = UPDATE_INSTALLED_CORES_ITERATE;
            }
         }
         break;
      case UPDATE_INSTALLED_CORES_END:
         {
            /* Set final task title */
            task_free_title(task);

            /* > Check whether core list was fetched
             *   successfully */
            if (update_installed_handle->list_size > 0)
            {
               char task_title[PATH_MAX_LENGTH];

               task_title[0] = '\0';

               /* > Generate final status message based on number
                *   of cores that were updated/locked */
               if (update_installed_handle->num_updated > 0)
               {
                  if (update_installed_handle->num_locked > 0)
                     snprintf(
                           task_title, sizeof(task_title), "%s [%s%u, %s%u]",
                           msg_hash_to_str(MSG_ALL_CORES_UPDATED),
                           msg_hash_to_str(MSG_NUM_CORES_UPDATED),
                           update_installed_handle->num_updated,
                           msg_hash_to_str(MSG_NUM_CORES_LOCKED),
                           update_installed_handle->num_locked);
                  else
                     snprintf(
                           task_title, sizeof(task_title), "%s [%s%u]",
                           msg_hash_to_str(MSG_ALL_CORES_UPDATED),
                           msg_hash_to_str(MSG_NUM_CORES_UPDATED),
                           update_installed_handle->num_updated);
               }
               else if (update_installed_handle->num_locked > 0)
                  snprintf(
                        task_title, sizeof(task_title), "%s [%s%u]",
                        msg_hash_to_str(MSG_ALL_CORES_UPDATED),
                        msg_hash_to_str(MSG_NUM_CORES_LOCKED),
                        update_installed_handle->num_locked);
               else
                  strlcpy(task_title, msg_hash_to_str(MSG_ALL_CORES_UPDATED),
                        sizeof(task_title));

               task_set_title(task, strdup(task_title));
            }
            else
               task_set_title(task, strdup(msg_hash_to_str(MSG_CORE_LIST_FAILED)));
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

   free_update_installed_cores_handle(update_installed_handle);
}

static bool task_update_installed_cores_finder(retro_task_t *task, void *user_data)
{
   if (!task)
      return false;

   if (task->handler == task_update_installed_cores_handler)
      return true;

   return false;
}

void task_push_update_installed_cores(
      bool auto_backup, size_t auto_backup_history_size,
      const char *path_dir_libretro,
      const char *path_dir_core_assets)
{
   task_finder_data_t find_data;
   retro_task_t *task                                       = NULL;
   update_installed_cores_handle_t *update_installed_handle =
         (update_installed_cores_handle_t*)
               calloc(1, sizeof(update_installed_cores_handle_t));

#if defined(ANDROID)
   /* Regular core updater is disabled in
    * Play Store builds */
   if (play_feature_delivery_enabled())
      goto error;
#endif

   /* Sanity check */
   if (!update_installed_handle ||
       string_is_empty(path_dir_libretro))
      goto error;

   /* Configure handle */
   update_installed_handle->auto_backup              = auto_backup;
   update_installed_handle->auto_backup_history_size = auto_backup_history_size;
   update_installed_handle->path_dir_libretro        = strdup(path_dir_libretro);
   update_installed_handle->path_dir_core_assets     = string_is_empty(path_dir_core_assets) ?
         NULL : strdup(path_dir_core_assets);
   update_installed_handle->core_list                = core_updater_list_init();
   update_installed_handle->list_task                = NULL;
   update_installed_handle->download_task            = NULL;
   update_installed_handle->list_size                = 0;
   update_installed_handle->list_index               = 0;
   update_installed_handle->installed_index          = 0;
   update_installed_handle->num_updated              = 0;
   update_installed_handle->num_locked               = 0;
   update_installed_handle->status                   = UPDATE_INSTALLED_CORES_BEGIN;

   if (!update_installed_handle->core_list)
      goto error;

   /* Only one instance of this task may run at a time */
   find_data.func     = task_update_installed_cores_finder;
   find_data.userdata = NULL;

   if (task_queue_find(&find_data))
      goto error;

   /* Create task */
   task = task_init();

   if (!task)
      goto error;

   /* Configure task */
   task->handler          = task_update_installed_cores_handler;
   task->state            = update_installed_handle;
   task->title            = strdup(msg_hash_to_str(MSG_FETCHING_CORE_LIST));
   task->alternative_look = true;
   task->progress         = 0;

   /* Push task */
   task_queue_push(task);

   return;

error:

   /* Clean up task */
   if (task)
   {
      free(task);
      task = NULL;
   }

   /* Clean up handle */
   free_update_installed_cores_handle(update_installed_handle);
}

#if defined(ANDROID)
/**************************************/
/* Play feature delivery core install */
/**************************************/

static void free_play_feature_delivery_install_handle(
      play_feature_delivery_install_handle_t *pfd_install_handle)
{
   if (!pfd_install_handle)
      return;

   if (pfd_install_handle->core_filename)
      free(pfd_install_handle->core_filename);

   if (pfd_install_handle->local_core_path)
      free(pfd_install_handle->local_core_path);

   if (pfd_install_handle->backup_core_path)
      free(pfd_install_handle->backup_core_path);

   if (pfd_install_handle->display_name)
      free(pfd_install_handle->display_name);

   free(pfd_install_handle);
   pfd_install_handle = NULL;
}

static void task_play_feature_delivery_core_install_handler(retro_task_t *task)
{
   play_feature_delivery_install_handle_t *pfd_install_handle = NULL;

   if (!task)
      goto task_finished;

   pfd_install_handle = (play_feature_delivery_install_handle_t*)task->state;

   if (!pfd_install_handle)
      goto task_finished;

   if (task_get_cancelled(task))
      goto task_finished;

   switch (pfd_install_handle->status)
   {
      case PLAY_FEATURE_DELIVERY_INSTALL_BEGIN:
         {
            /* Check whether core has already been
             * installed via play feature delivery */
            if (play_feature_delivery_core_installed(
                  pfd_install_handle->core_filename))
            {
               pfd_install_handle->success                = true;
               pfd_install_handle->core_already_installed = true;
               pfd_install_handle->status                 =
                     PLAY_FEATURE_DELIVERY_INSTALL_END;
               break;
            }

            /* If core is already installed via other
             * means, must remove it before attempting
             * play feature delivery transaction */
            if (path_is_valid(pfd_install_handle->local_core_path))
            {
               char backup_core_path[PATH_MAX_LENGTH];
               bool backup_successful = false;

               backup_core_path[0] = '\0';

               /* Have to create a backup, in case install
                * process fails
                * > Note: since only one install task can
                *   run at a time, a UID is not required */

               /* Generate backup file name */
               strlcpy(backup_core_path, pfd_install_handle->local_core_path,
                     sizeof(backup_core_path));
               strlcat(backup_core_path, FILE_PATH_BACKUP_EXTENSION,
                     sizeof(backup_core_path));

               if (!string_is_empty(backup_core_path))
               {
                  int ret;

                  /* If an old backup file exists (i.e. leftovers
                   * from a mid-task crash/user exit), delete it */
                  if (path_is_valid(backup_core_path))
                     filestream_delete(backup_core_path);

                  /* Attempt to rename core file */
                  ret = filestream_rename(
                        pfd_install_handle->local_core_path,
                        backup_core_path);

                  if (!ret)
                  {
                     /* Success - cache backup file name */
                     pfd_install_handle->backup_core_path = strdup(backup_core_path);
                     backup_successful                    = true;
                  }
               }

               /* If backup failed, all we can do is delete
                * the existing core file... */
               if (!backup_successful &&
                   path_is_valid(pfd_install_handle->local_core_path))
                  filestream_delete(pfd_install_handle->local_core_path);
            }

            /* Start download */
            if (play_feature_delivery_download(
                  pfd_install_handle->core_filename))
               pfd_install_handle->status = PLAY_FEATURE_DELIVERY_INSTALL_WAIT;
            else
               pfd_install_handle->status = PLAY_FEATURE_DELIVERY_INSTALL_END;
         }
         break;
      case PLAY_FEATURE_DELIVERY_INSTALL_WAIT:
         {
            bool install_active;
            enum play_feature_delivery_install_status install_status;
            unsigned install_progress;
            char task_title[PATH_MAX_LENGTH];

            task_title[0] = '\0';

            /* Get current install status */
            install_active = play_feature_delivery_download_status(
                  &install_status, &install_progress);

            /* In all cases, update task progress */
            task_set_progress(task, install_progress);

            /* Interpret status */
            switch (install_status)
            {
               case PLAY_FEATURE_DELIVERY_INSTALLED:
                  pfd_install_handle->success = true;
                  pfd_install_handle->status  = PLAY_FEATURE_DELIVERY_INSTALL_END;
                  break;
               case PLAY_FEATURE_DELIVERY_FAILED:
                  pfd_install_handle->status  = PLAY_FEATURE_DELIVERY_INSTALL_END;
                  break;
               case PLAY_FEATURE_DELIVERY_DOWNLOADING:
                  task_free_title(task);
                  strlcpy(task_title, msg_hash_to_str(MSG_DOWNLOADING_CORE),
                        sizeof(task_title));
                  strlcat(task_title, pfd_install_handle->display_name,
                        sizeof(task_title));
                  task_set_title(task, strdup(task_title));
                  break;
               case PLAY_FEATURE_DELIVERY_INSTALLING:
                  task_free_title(task);
                  strlcpy(task_title, msg_hash_to_str(MSG_INSTALLING_CORE),
                        sizeof(task_title));
                  strlcat(task_title, pfd_install_handle->display_name,
                        sizeof(task_title));
                  task_set_title(task, strdup(task_title));
                  break;
               default:
                  break;
            }

            /* If install is inactive, end task (regardless
             * of status) */
            if (!install_active)
               pfd_install_handle->status = PLAY_FEATURE_DELIVERY_INSTALL_END;
         }
         break;
      case PLAY_FEATURE_DELIVERY_INSTALL_END:
         {
            const char *msg_str = msg_hash_to_str(MSG_CORE_INSTALL_FAILED);
            char task_title[PATH_MAX_LENGTH];

            task_title[0] = '\0';

            /* Set final task title */
            task_free_title(task);

            if (pfd_install_handle->success)
               msg_str = pfd_install_handle->core_already_installed ?
                     msg_hash_to_str(MSG_LATEST_CORE_INSTALLED) :
                     msg_hash_to_str(MSG_CORE_INSTALLED);

            strlcpy(task_title, msg_str, sizeof(task_title));
            strlcat(task_title, pfd_install_handle->display_name,
                  sizeof(task_title));

            task_set_title(task, strdup(task_title));

            /* Check whether a core backup file was created */
            if (!string_is_empty(pfd_install_handle->backup_core_path) &&
                path_is_valid(pfd_install_handle->backup_core_path))
            {
               /* If install was successful, delete backup */
               if (pfd_install_handle->success)
                  filestream_delete(pfd_install_handle->backup_core_path);
               else
               {
                  /* Otherwise, attempt to restore backup */
                  int ret = filestream_rename(
                        pfd_install_handle->backup_core_path,
                        pfd_install_handle->local_core_path);

                  /* If restore failed, all we can do is attempt
                   * to delete the backup... */
                  if (ret && path_is_valid(pfd_install_handle->backup_core_path))
                     filestream_delete(pfd_install_handle->backup_core_path);
               }
            }

            /* If task is muted and install failed, set
             * error string (allows status to be checked
             * externally) */
            if (!pfd_install_handle->success &&
                task_get_mute(task))
               task_set_error(task, strdup(task_title));
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

   free_play_feature_delivery_install_handle(pfd_install_handle);
}

static bool task_play_feature_delivery_core_install_finder(
      retro_task_t *task, void *user_data)
{
   if (!task)
      return false;

   if (task->handler == task_play_feature_delivery_core_install_handler)
      return true;

   return false;
}

void *task_push_play_feature_delivery_core_install(
      core_updater_list_t* core_list,
      const char *filename,
      bool mute)
{
   task_finder_data_t find_data;
   char task_title[PATH_MAX_LENGTH];
   const core_updater_list_entry_t *list_entry                = NULL;
   retro_task_t *task                                         = NULL;
   play_feature_delivery_install_handle_t *pfd_install_handle = (play_feature_delivery_install_handle_t*)
         calloc(1, sizeof(play_feature_delivery_install_handle_t));

   task_title[0] = '\0';

   /* Sanity check */
   if (!core_list ||
       string_is_empty(filename) ||
       !pfd_install_handle ||
       !play_feature_delivery_enabled())
      goto error;

   /* Get core updater list entry */
   if (!core_updater_list_get_filename(
         core_list, filename, &list_entry))
      goto error;

   if (string_is_empty(list_entry->local_core_path) ||
       string_is_empty(list_entry->display_name))
      goto error;

   /* Only one core may be downloaded at a time */
   find_data.func     = task_play_feature_delivery_core_install_finder;
   find_data.userdata = NULL;

   if (task_queue_find(&find_data))
      goto error;

   /* Configure handle */
   pfd_install_handle->core_filename          = strdup(list_entry->remote_filename);
   pfd_install_handle->local_core_path        = strdup(list_entry->local_core_path);
   pfd_install_handle->backup_core_path       = NULL;
   pfd_install_handle->display_name           = strdup(list_entry->display_name);
   pfd_install_handle->success                = false;
   pfd_install_handle->core_already_installed = false;
   pfd_install_handle->status                 = PLAY_FEATURE_DELIVERY_INSTALL_BEGIN;

   /* Create task */
   task = task_init();

   if (!task)
      goto error;

   /* Configure task */
   strlcpy(task_title, msg_hash_to_str(MSG_UPDATING_CORE),
         sizeof(task_title));
   strlcat(task_title, pfd_install_handle->display_name,
         sizeof(task_title));

   task->handler          = task_play_feature_delivery_core_install_handler;
   task->state            = pfd_install_handle;
   task->mute             = mute;
   task->title            = strdup(task_title);
   task->alternative_look = true;
   task->progress         = 0;
   task->callback         = cb_task_core_updater_download;

   /* Install process may involve the *deletion*
    * of an existing core file. If core is
    * already running, must therefore unload it
    * to prevent undefined behaviour */
   if (retroarch_ctl(RARCH_CTL_IS_CORE_LOADED, (void*)list_entry->local_core_path))
      command_event(CMD_EVENT_UNLOAD_CORE, NULL);

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
   free_play_feature_delivery_install_handle(pfd_install_handle);

   return NULL;
}

/************************************************/
/* Play feature delivery switch installed cores */
/************************************************/

static void free_play_feature_delivery_switch_cores_handle(
      play_feature_delivery_switch_cores_handle_t *pfd_switch_cores_handle)
{
   if (!pfd_switch_cores_handle)
      return;

   if (pfd_switch_cores_handle->path_dir_libretro)
      free(pfd_switch_cores_handle->path_dir_libretro);

   if (pfd_switch_cores_handle->path_libretro_info)
      free(pfd_switch_cores_handle->path_libretro_info);

   if (pfd_switch_cores_handle->error_msg)
      free(pfd_switch_cores_handle->error_msg);

   core_updater_list_free(pfd_switch_cores_handle->core_list);

   free(pfd_switch_cores_handle);
   pfd_switch_cores_handle = NULL;
}

static void task_play_feature_delivery_switch_cores_handler(retro_task_t *task)
{
   play_feature_delivery_switch_cores_handle_t *pfd_switch_cores_handle = NULL;

   if (!task)
      goto task_finished;

   pfd_switch_cores_handle = (play_feature_delivery_switch_cores_handle_t*)task->state;

   if (!pfd_switch_cores_handle)
      goto task_finished;

   if (task_get_cancelled(task))
      goto task_finished;

   switch (pfd_switch_cores_handle->status)
   {
      case PLAY_FEATURE_DELIVERY_SWITCH_CORES_BEGIN:
         {
            /* Query available cores
             * Note: It should never be possible for this
             * function (or the subsequent parsing of its
             * output) to fail. We handle error conditions
             * regardless, but there is no need to perform
             * detailed checking - just report any problems
             * to the user as a generic 'failed to retrieve
             * core list' error */
            struct string_list *available_cores =
                  play_feature_delivery_available_cores();
            bool success                        = false;

            if (!available_cores)
            {
               pfd_switch_cores_handle->status =
                     PLAY_FEATURE_DELIVERY_SWITCH_CORES_END;
               break;
            }

            /* Populate core updater list */
            success = core_updater_list_parse_pfd_data(
                  pfd_switch_cores_handle->core_list,
                  pfd_switch_cores_handle->path_dir_libretro,
                  pfd_switch_cores_handle->path_libretro_info,
                  available_cores);

            string_list_free(available_cores);

            /* Cache list size */
            if (success)
               pfd_switch_cores_handle->list_size =
                     core_updater_list_size(pfd_switch_cores_handle->core_list);

            if (pfd_switch_cores_handle->list_size < 1)
               pfd_switch_cores_handle->status =
                     PLAY_FEATURE_DELIVERY_SWITCH_CORES_END;
            else
               pfd_switch_cores_handle->status =
                     PLAY_FEATURE_DELIVERY_SWITCH_CORES_ITERATE;
         }
         break;
      case PLAY_FEATURE_DELIVERY_SWITCH_CORES_ITERATE:
         {
            const core_updater_list_entry_t *list_entry = NULL;
            bool core_installed                         = false;

            /* Check whether we have reached the end
             * of the list */
            if (pfd_switch_cores_handle->list_index >=
                  pfd_switch_cores_handle->list_size)
            {
               pfd_switch_cores_handle->status =
                     PLAY_FEATURE_DELIVERY_SWITCH_CORES_END;
               break;
            }

            /* Check whether current core is installed */
            if (core_updater_list_get_index(
                  pfd_switch_cores_handle->core_list,
                  pfd_switch_cores_handle->list_index,
                  &list_entry) &&
                path_is_valid(list_entry->local_core_path))
            {
               core_installed                           = true;
               pfd_switch_cores_handle->installed_index =
                     pfd_switch_cores_handle->list_index;
               pfd_switch_cores_handle->status          =
                     PLAY_FEATURE_DELIVERY_SWITCH_CORES_INSTALL_CORE;
            }

            /* Update progress display */
            task_free_title(task);

            if (core_installed)
            {
               char task_title[PATH_MAX_LENGTH];

               task_title[0] = '\0';

               strlcpy(task_title, msg_hash_to_str(MSG_CHECKING_CORE),
                     sizeof(task_title));
               strlcat(task_title, list_entry->display_name,
                     sizeof(task_title));

               task_set_title(task, strdup(task_title));
            }
            else
               task_set_title(task, strdup(msg_hash_to_str(MSG_SCANNING_CORES)));

            task_set_progress(task,
                  (pfd_switch_cores_handle->list_index * 100) /
                        pfd_switch_cores_handle->list_size);

            /* Increment list index */
            pfd_switch_cores_handle->list_index++;
         }
         break;
      case PLAY_FEATURE_DELIVERY_SWITCH_CORES_INSTALL_CORE:
         {
            const core_updater_list_entry_t *list_entry = NULL;

            /* Get list entry
             * > In the event of an error, just return
             *   to PLAY_FEATURE_DELIVERY_SWITCH_CORES_ITERATE
             *   state */
            if (!core_updater_list_get_index(
                  pfd_switch_cores_handle->core_list,
                  pfd_switch_cores_handle->installed_index,
                  &list_entry))
            {
               pfd_switch_cores_handle->status =
                     PLAY_FEATURE_DELIVERY_SWITCH_CORES_ITERATE;
               break;
            }

            /* Check whether core is already installed via
             * play feature delivery */
            if (play_feature_delivery_core_installed(
                  list_entry->remote_filename))
            {
               pfd_switch_cores_handle->status =
                     PLAY_FEATURE_DELIVERY_SWITCH_CORES_ITERATE;
               break;
            }

            /* Existing core is not installed via
             * play feature delivery
             * > Request installation/replacement */
            pfd_switch_cores_handle->install_task = (retro_task_t*)
                  task_push_play_feature_delivery_core_install(
                        pfd_switch_cores_handle->core_list,
                        list_entry->remote_filename,
                        true);

            /* Again, if an error occurred, just return to
             * PLAY_FEATURE_DELIVERY_SWITCH_CORES_ITERATE
             * state */
            if (!pfd_switch_cores_handle->install_task)
               pfd_switch_cores_handle->status =
                     PLAY_FEATURE_DELIVERY_SWITCH_CORES_ITERATE;
            else
            {
               char task_title[PATH_MAX_LENGTH];

               task_title[0] = '\0';

               /* Update task title */
               task_free_title(task);

               strlcpy(task_title, msg_hash_to_str(MSG_UPDATING_CORE),
                     sizeof(task_title));
               strlcat(task_title, list_entry->display_name,
                     sizeof(task_title));

               task_set_title(task, strdup(task_title));

               /* Wait for installation to complete */
               pfd_switch_cores_handle->status =
                     PLAY_FEATURE_DELIVERY_SWITCH_CORES_WAIT_INSTALL;
            }
         }
         break;
      case PLAY_FEATURE_DELIVERY_SWITCH_CORES_WAIT_INSTALL:
         {
            bool install_complete = false;
            const char* error_msg = NULL;

            /* > If task is running, check 'is finished' status
             * > If task is NULL, then it is finished by
             *   definition */
            if (pfd_switch_cores_handle->install_task)
            {
               error_msg        = task_get_error(
                     pfd_switch_cores_handle->install_task);
               install_complete = task_get_finished(
                     pfd_switch_cores_handle->install_task);
            }
            else
               install_complete = true;

            /* Check for installation errors
             * > These should be considered 'serious', and
             *   will trigger the task to end early */
            if (!string_is_empty(error_msg))
            {
               pfd_switch_cores_handle->error_msg    = strdup(error_msg);
               pfd_switch_cores_handle->install_task = NULL;
               pfd_switch_cores_handle->status       =
                     PLAY_FEATURE_DELIVERY_SWITCH_CORES_END;
               break;
            }

            /* If installation is complete, return to
             * PLAY_FEATURE_DELIVERY_SWITCH_CORES_ITERATE
             * state */
            if (install_complete)
            {
               pfd_switch_cores_handle->install_task = NULL;
               pfd_switch_cores_handle->status       =
                     PLAY_FEATURE_DELIVERY_SWITCH_CORES_ITERATE;
            }
         }
         break;
      case PLAY_FEATURE_DELIVERY_SWITCH_CORES_END:
         {
            const char *task_title = msg_hash_to_str(MSG_CORE_LIST_FAILED);

            /* Set final task title */
            task_free_title(task);

            /* > Check whether core list was generated
             *   successfully */
            if (pfd_switch_cores_handle->list_size > 0)
            {
               /* Check whether any installation errors occurred */
               if (!string_is_empty(pfd_switch_cores_handle->error_msg))
                  task_title = pfd_switch_cores_handle->error_msg;
               else
                  task_title = msg_hash_to_str(MSG_ALL_CORES_SWITCHED_PFD);
            }

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

   free_play_feature_delivery_switch_cores_handle(pfd_switch_cores_handle);
}

static bool task_play_feature_delivery_switch_cores_finder(
      retro_task_t *task, void *user_data)
{
   if (!task)
      return false;

   if (task->handler == task_play_feature_delivery_switch_cores_handler)
      return true;

   return false;
}

void task_push_play_feature_delivery_switch_installed_cores(
      const char *path_dir_libretro,
      const char *path_libretro_info)
{
   task_finder_data_t find_data;
   retro_task_t *task                                                   = NULL;
   play_feature_delivery_switch_cores_handle_t *pfd_switch_cores_handle =
         (play_feature_delivery_switch_cores_handle_t*)
               calloc(1, sizeof(play_feature_delivery_switch_cores_handle_t));

   /* Sanity check */
   if (string_is_empty(path_dir_libretro) ||
       string_is_empty(path_libretro_info) ||
       !pfd_switch_cores_handle ||
       !play_feature_delivery_enabled())
      goto error;

   /* Only one instance of this task my run at a time */
   find_data.func     = task_play_feature_delivery_switch_cores_finder;
   find_data.userdata = NULL;

   if (task_queue_find(&find_data))
      goto error;

   /* Configure handle */
   pfd_switch_cores_handle->path_dir_libretro  = strdup(path_dir_libretro);
   pfd_switch_cores_handle->path_libretro_info = strdup(path_libretro_info);
   pfd_switch_cores_handle->error_msg          = NULL;
   pfd_switch_cores_handle->core_list          = core_updater_list_init();
   pfd_switch_cores_handle->install_task       = NULL;
   pfd_switch_cores_handle->list_size          = 0;
   pfd_switch_cores_handle->list_index         = 0;
   pfd_switch_cores_handle->installed_index    = 0;
   pfd_switch_cores_handle->status             = PLAY_FEATURE_DELIVERY_SWITCH_CORES_BEGIN;

   if (!pfd_switch_cores_handle->core_list)
      goto error;

   /* Create task */
   task = task_init();

   if (!task)
      goto error;

   /* Configure task */
   task->handler          = task_play_feature_delivery_switch_cores_handler;
   task->state            = pfd_switch_cores_handle;
   task->title            = strdup(msg_hash_to_str(MSG_SCANNING_CORES));
   task->alternative_look = true;
   task->progress         = 0;

   /* Push task */
   task_queue_push(task);

   return;

error:

   /* Clean up task */
   if (task)
   {
      free(task);
      task = NULL;
   }

   /* Clean up handle */
   free_play_feature_delivery_switch_cores_handle(pfd_switch_cores_handle);
}

#endif
