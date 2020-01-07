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
#include <streams/file_stream.h>
#include <encodings/crc32.h>

#include "task_file_transfer.h"
#include "tasks_internal.h"

#include "../configuration.h"
#include "../retroarch.h"
#include "../command.h"
#include "../msg_hash.h"
#include "../verbosity.h"
#include "../core_updater_list.h"

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
   bool refresh_menu;
   retro_task_t *http_task;
   bool http_task_finished;
   bool http_task_complete;
   http_transfer_data_t *http_data;
   enum core_updater_list_status status;
} core_updater_list_handle_t;

/* Download core */
enum core_updater_download_status
{
   CORE_UPDATER_DOWNLOAD_BEGIN = 0,
   CORE_UPDATER_DOWNLOAD_WAIT_TRANSFER,
   CORE_UPDATER_DOWNLOAD_WAIT_DECOMPRESS,
   CORE_UPDATER_DOWNLOAD_END
};

typedef struct core_updater_download_handle
{
   char *remote_filename;
   char *remote_core_path;
   char *local_download_path;
   char *local_core_path;
   char *display_name;
   uint32_t remote_crc;
   bool check_crc;
   bool crc_match;
   retro_task_t *http_task;
   bool http_task_finished;
   bool http_task_complete;
   retro_task_t *decompress_task;
   bool decompress_task_finished;
   bool decompress_task_complete;
   enum core_updater_download_status status;
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
   core_updater_list_t* core_list;
   retro_task_t *list_task;
   retro_task_t *download_task;
   size_t list_size;
   size_t list_index;
   size_t installed_index;
   unsigned num_updated;
   enum update_installed_cores_status status;
} update_installed_cores_handle_t;

/*********************/
/* Utility functions */
/*********************/

/* Returns true if local core has the same crc
 * value as core on buildbot */
static bool local_core_matches_remote_crc(
      const char *local_core_path, uint32_t remote_crc)
{
   /* Sanity check */
   if (string_is_empty(local_core_path) || (remote_crc == 0))
      return false;

   if (path_is_valid(local_core_path))
   {
      int64_t length   = 0;
      uint8_t *ret_buf = NULL;

      if (filestream_read_file(
            local_core_path, (void**)&ret_buf, &length))
      {
         uint32_t crc = 0;

         if (length >= 0)
            crc = encoding_crc32(0, ret_buf, length);

         if (ret_buf)
            free(ret_buf);

         if ((crc != 0) && (crc == remote_crc))
            return true;
      }
   }

   return false;
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

   if (!data || !transf || err)
      goto finish;

   list_handle = (core_updater_list_handle_t*)transf->user_data;

   if (!list_handle)
      goto finish;

   list_handle->http_data          = data;
   list_handle->http_task_complete = true;

finish:

   if (transf)
      free(transf);
}

static void free_core_updater_list_handle(core_updater_list_handle_t *list_handle)
{
   if (!list_handle)
      return;

   if (list_handle->http_data)
   {
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

            buildbot_url[0] = '\0';

            /* Reset core updater list */
            core_updater_list_reset(list_handle->core_list);

            /* Get core listing URL */
            if (!settings)
               goto task_finished;

            if (string_is_empty(settings->paths.network_buildbot_url))
               goto task_finished;

            fill_pathname_join(
                  buildbot_url,
                  settings->paths.network_buildbot_url,
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
            list_handle->http_task = (retro_task_t*)task_push_http_transfer(
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

            /* Wait for task_push_http_transfer()
             * callback to trigger */
            if (list_handle->http_task_complete)
               list_handle->status = CORE_UPDATER_LIST_END;
         }
         break;
      case CORE_UPDATER_LIST_END:
         {
            settings_t *settings    = config_get_ptr();

            /* Parse HTTP transfer data */
            if (list_handle->http_data)
               core_updater_list_parse_network_data(
                     list_handle->core_list,
                     settings->paths.directory_libretro,
                     settings->paths.path_libretro_info,
                     settings->paths.network_buildbot_url,
                     list_handle->http_data->data,
                     list_handle->http_data->len);

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

   /* Sanity check */
   if (!core_list || !list_handle)
      goto error;

   /* Configure handle */
   list_handle->core_list          = core_list;
   list_handle->refresh_menu       = refresh_menu;
   list_handle->http_task          = NULL;
   list_handle->http_task_finished = false;
   list_handle->http_task_complete = false;
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
      RARCH_ERR("%s", err);
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
   {
      RARCH_ERR("Download of '%s' failed: %s\n",
            (transf ? transf->path: "unknown"), err);
   }

   if (data)
   {
      if (data->data)
         free(data->data);
      free(data);
   }

   if (transf)
      free(transf);
}

static void free_core_updater_download_handle(core_updater_download_handle_t *download_handle)
{
   if (!download_handle)
      return;

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
            file_transfer_t *transf = NULL;

            /* Check CRC of existing core, if required */
            if (download_handle->check_crc)
               download_handle->crc_match = local_core_matches_remote_crc(
                     download_handle->local_core_path,
                     download_handle->remote_crc);

            /* If CRC matches, end task immediately */
            if (download_handle->crc_match)
            {
               download_handle->status = CORE_UPDATER_DOWNLOAD_END;
               break;
            }

            /* Configure file transfer object */
            transf = (file_transfer_t*)calloc(1, sizeof(file_transfer_t));

            if (!transf)
               goto task_finished;

            strlcpy(
                  transf->path, download_handle->local_download_path,
                  sizeof(transf->path));

            transf->user_data = (void*)download_handle;

            /* Push HTTP transfer task */
            download_handle->http_task = (retro_task_t*)task_push_http_transfer(
                  download_handle->remote_core_path, true, NULL,
                  cb_http_task_core_updater_download, transf);

            /* Start waiting for HTTP transfer to complete */
            download_handle->status = CORE_UPDATER_DOWNLOAD_WAIT_TRANSFER;
         }
         break;
      case CORE_UPDATER_DOWNLOAD_WAIT_TRANSFER:
         {
            char task_title[PATH_MAX_LENGTH];

            task_title[0] = '\0';

            /* Update task title */
            task_free_title(task);

            strlcpy(
                  task_title, msg_hash_to_str(MSG_DOWNLOADING_CORE),
                  sizeof(task_title));
            strlcat(task_title, download_handle->display_name, sizeof(task_title));

            task_set_title(task, strdup(task_title));

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
                  /* Download accounts for first half of
                   * task progress */
                  int8_t progress = task_get_progress(download_handle->http_task);

                  task_set_progress(task, progress >> 1);
               }
            }

            /* Wait for task_push_http_transfer()
             * callback to trigger */
            if (download_handle->http_task_complete)
               download_handle->status = CORE_UPDATER_DOWNLOAD_WAIT_DECOMPRESS;
         }
         break;
      case CORE_UPDATER_DOWNLOAD_WAIT_DECOMPRESS:
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
                  /* Download accounts for second half
                   * of task progress */
                  int8_t progress = task_get_progress(download_handle->decompress_task);

                  task_set_progress(task, 50 + (progress >> 1));
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

            /* Reload core info files */
            command_event(CMD_EVENT_CORE_INFO_INIT, NULL);

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
      core_updater_list_t* core_list, const char *filename, bool mute, bool check_crc)
{
   task_finder_data_t find_data;
   char task_title[PATH_MAX_LENGTH];
   char local_download_path[PATH_MAX_LENGTH];
   settings_t *settings                            = config_get_ptr();
   const core_updater_list_entry_t *list_entry     = NULL;
   retro_task_t *task                              = NULL;
   core_updater_download_handle_t *download_handle = (core_updater_download_handle_t*)
         calloc(1, sizeof(core_updater_download_handle_t));

   task_title[0]          = '\0';
   local_download_path[0] = '\0';

   /* Sanity check */
   if (!core_list ||
       string_is_empty(filename) ||
       !settings ||
       !download_handle)
      goto error;

   /* Get core updater list entry */
   if (!core_updater_list_get_filename(
         core_list, filename, &list_entry))
      goto error;

   if (string_is_empty(list_entry->remote_core_path))
      goto error;

   if (string_is_empty(list_entry->local_core_path))
      goto error;

   if (string_is_empty(list_entry->display_name))
      goto error;

   /* Get local file download path */
   if (string_is_empty(settings->paths.directory_libretro))
      goto error;

   fill_pathname_join(
         local_download_path,
         settings->paths.directory_libretro,
         list_entry->remote_filename,
         sizeof(local_download_path));

   /* Configure handle */
   download_handle->remote_filename          = strdup(list_entry->remote_filename);
   download_handle->remote_core_path         = strdup(list_entry->remote_core_path);
   download_handle->local_download_path      = strdup(local_download_path);
   download_handle->local_core_path          = strdup(list_entry->local_core_path);
   download_handle->display_name             = strdup(list_entry->display_name);
   download_handle->remote_crc               = list_entry->crc;
   download_handle->check_crc                = check_crc;
   download_handle->crc_match                = false;
   download_handle->http_task                = NULL;
   download_handle->http_task_finished       = false;
   download_handle->http_task_complete       = false;
   download_handle->decompress_task          = NULL;
   download_handle->decompress_task_finished = false;
   download_handle->decompress_task_complete = false;
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

static void free_update_installed_cores_handle(update_installed_cores_handle_t *update_installed_handle)
{
   if (!update_installed_handle)
      return;

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
         {
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
         }
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
            bool crc_match;

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

            /* Check CRC of existing core */
            crc_match = local_core_matches_remote_crc(
                  list_entry->local_core_path,
                  list_entry->crc);

            /* If CRC matches, then core is already the most
             * recent version - just return to
             * UPDATE_INSTALLED_CORES_ITERATE state */
            if (crc_match)
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
                        true, false);

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
               /* > Check whether a non-zero number of cores
                *   were updated */
               if (update_installed_handle->num_updated > 0)
               {
                  char task_title[PATH_MAX_LENGTH];

                  task_title[0] = '\0';

                  snprintf(
                        task_title, sizeof(task_title), "%s [%s%u]",
                        msg_hash_to_str(MSG_ALL_CORES_UPDATED),
                        msg_hash_to_str(MSG_NUM_CORES_UPDATED),
                        update_installed_handle->num_updated);

                  task_set_title(task, strdup(task_title));
               }
               else
                  task_set_title(task, strdup(msg_hash_to_str(MSG_ALL_CORES_UPDATED)));
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

void task_push_update_installed_cores(void)
{
   task_finder_data_t find_data;
   retro_task_t *task                                       = NULL;
   update_installed_cores_handle_t *update_installed_handle =
         (update_installed_cores_handle_t*)
               calloc(1, sizeof(update_installed_cores_handle_t));

   /* Sanity check */
   if (!update_installed_handle)
      goto error;

   /* Configure handle */
   update_installed_handle->core_list       = core_updater_list_init(CORE_UPDATER_LIST_SIZE);
   update_installed_handle->list_task       = NULL;
   update_installed_handle->download_task   = NULL;
   update_installed_handle->list_size       = 0;
   update_installed_handle->list_index      = 0;
   update_installed_handle->installed_index = 0;
   update_installed_handle->num_updated     = 0;
   update_installed_handle->status          = UPDATE_INSTALLED_CORES_BEGIN;

   if (!update_installed_handle->core_list)
      goto error;

   /* Only one instance of this task may run at a time */
   find_data.func = task_update_installed_cores_finder;

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
