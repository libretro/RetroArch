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
#include <lists/string_list.h>
#include <file/file_path.h>
#include <formats/logiqx_dat.h>

#include "tasks_internal.h"

#include "../configuration.h"
#include "../retroarch.h"
#include "../msg_hash.h"
#include "../playlist.h"
#include "../manual_content_scan.h"

#ifdef RARCH_INTERNAL
#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#endif
#endif

enum manual_scan_status
{
   MANUAL_SCAN_BEGIN = 0,
   MANUAL_SCAN_ITERATE_CONTENT,
   MANUAL_SCAN_END
};

typedef struct manual_scan_handle
{
   manual_content_scan_task_config_t *task_config;
   playlist_t *playlist;
   struct string_list *content_list;
   logiqx_dat_t *dat_file;
   size_t list_size;
   size_t list_index;
   enum manual_scan_status status;
   bool fuzzy_archive_match;
   bool use_old_format;
} manual_scan_handle_t;

/* Frees task handle + all constituent objects */
static void free_manual_content_scan_handle(manual_scan_handle_t *manual_scan)
{
   if (!manual_scan)
      return;

   if (manual_scan->task_config)
   {
      free(manual_scan->task_config);
      manual_scan->task_config = NULL;
   }

   if (manual_scan->playlist)
   {
      playlist_free(manual_scan->playlist);
      manual_scan->playlist = NULL;
   }

   if (manual_scan->content_list)
   {
      string_list_free(manual_scan->content_list);
      manual_scan->content_list = NULL;
   }

   if (manual_scan->dat_file)
   {
      logiqx_dat_free(manual_scan->dat_file);
      manual_scan->dat_file = NULL;
   }

   free(manual_scan);
   manual_scan = NULL;
}

static void task_manual_content_scan_handler(retro_task_t *task)
{
   manual_scan_handle_t *manual_scan = NULL;

   if (!task)
      goto task_finished;

   manual_scan = (manual_scan_handle_t*)task->state;

   if (!manual_scan)
      goto task_finished;

   if (task_get_cancelled(task))
      goto task_finished;

   switch (manual_scan->status)
   {
      case MANUAL_SCAN_BEGIN:
         {
            /* Get content list */
            manual_scan->content_list = manual_content_scan_get_content_list(
                  manual_scan->task_config);

            if (!manual_scan->content_list)
            {
               runloop_msg_queue_push(
                     msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_INVALID_CONTENT),
                     1, 100, true,
                     NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               goto task_finished;
            }

            manual_scan->list_size = manual_scan->content_list->size;

            /* Load DAT file, if required */
            if (!string_is_empty(manual_scan->task_config->dat_file_path))
            {
               manual_scan->dat_file =
                     logiqx_dat_init(manual_scan->task_config->dat_file_path);

               if (!manual_scan->dat_file)
               {
                  runloop_msg_queue_push(
                        msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_DAT_FILE_LOAD_ERROR),
                        1, 100, true,
                        NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
                  goto task_finished;
               }
            }

            /* Open playlist */
            manual_scan->playlist = playlist_init(
                  manual_scan->task_config->playlist_file, COLLECTION_SIZE);

            if (!manual_scan->playlist)
               goto task_finished;

            /* Reset playlist, if required */
            if (manual_scan->task_config->overwrite_playlist)
               playlist_clear(manual_scan->playlist);

            /* Set default core, if required */
            if (manual_scan->task_config->core_set)
            {
               playlist_set_default_core_path(
                     manual_scan->playlist, manual_scan->task_config->core_path);
               playlist_set_default_core_name(
                     manual_scan->playlist, manual_scan->task_config->core_name);
            }

            /* All good - can start iterating */
            manual_scan->status = MANUAL_SCAN_ITERATE_CONTENT;
         }
         break;
      case MANUAL_SCAN_ITERATE_CONTENT:
         {
            const char *content_path =
                  manual_scan->content_list->elems[manual_scan->list_index].data;
            int content_type         =
                  manual_scan->content_list->elems[manual_scan->list_index].attr.i;

            if (!string_is_empty(content_path))
            {
               const char *content_file = path_basename(content_path);
               char task_title[PATH_MAX_LENGTH];

               task_title[0] = '\0';

               /* Update progress display */
               task_free_title(task);

               strlcpy(
                     task_title, msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_IN_PROGRESS),
                     sizeof(task_title));

               if (!string_is_empty(content_file))
                  strlcat(task_title, content_file, sizeof(task_title));

               task_set_title(task, strdup(task_title));
               task_set_progress(task, (manual_scan->list_index * 100) / manual_scan->list_size);

               /* Add content to playlist */
               manual_content_scan_add_content_to_playlist(
                     manual_scan->task_config, manual_scan->playlist,
                     content_path, content_type, manual_scan->dat_file,
                     manual_scan->fuzzy_archive_match);
            }

            /* Increment content index */
            manual_scan->list_index++;
            if (manual_scan->list_index >= manual_scan->list_size)
               manual_scan->status = MANUAL_SCAN_END;
         }
         break;
      case MANUAL_SCAN_END:
         {
            playlist_t *cached_playlist = playlist_get_cached();
            char task_title[PATH_MAX_LENGTH];

            task_title[0] = '\0';

            /* Ensure playlist is alphabetically sorted */
            playlist_qsort(manual_scan->playlist);

            /* Save playlist changes to disk */
            playlist_write_file(manual_scan->playlist, manual_scan->use_old_format);

            /* If this is the currently cached playlist, then
             * it must be re-cached (otherwise changes will be
             * lost if the currently cached playlist is saved
             * to disk for any reason...) */
            if (cached_playlist)
            {
               if (string_is_equal(
                     manual_scan->task_config->playlist_file,
                     playlist_get_conf_path(cached_playlist)))
               {
                  playlist_free_cached();
                  playlist_init_cached(
                        manual_scan->task_config->playlist_file, COLLECTION_SIZE);
               }
            }

            /* Update progress display */
            task_free_title(task);

            strlcpy(
                  task_title, msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_END),
                  sizeof(task_title));
            strlcat(task_title, manual_scan->task_config->system_name,
                  sizeof(task_title));

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

   free_manual_content_scan_handle(manual_scan);
}

static bool task_manual_content_scan_finder(retro_task_t *task, void *user_data)
{
   manual_scan_handle_t *manual_scan = NULL;

   if (!task || !user_data)
      return false;

   if (task->handler != task_manual_content_scan_handler)
      return false;

   manual_scan = (manual_scan_handle_t*)task->state;
   if (!manual_scan)
      return false;

   return string_is_equal(
         (const char*)user_data, manual_scan->task_config->playlist_file);
}

static void cb_task_manual_content_scan_refresh_menu(
      retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
#if defined(RARCH_INTERNAL) && defined(HAVE_MENU)
   menu_ctx_environment_t menu_environ;
   menu_environ.type = MENU_ENVIRON_RESET_HORIZONTAL_LIST;
   menu_environ.data = NULL;

   menu_driver_ctl(RARCH_MENU_CTL_ENVIRONMENT, &menu_environ);
#endif
}

bool task_push_manual_content_scan(void)
{
   task_finder_data_t find_data;
   char task_title[PATH_MAX_LENGTH];
   retro_task_t *task                = NULL;
   settings_t *settings              = config_get_ptr();
   manual_scan_handle_t *manual_scan = (manual_scan_handle_t*)
         calloc(1, sizeof(manual_scan_handle_t));

   task_title[0] = '\0';

   /* Sanity check */
   if (!manual_scan)
      goto error;

   /* Configure handle */
   manual_scan->task_config         = NULL;
   manual_scan->playlist            = NULL;
   manual_scan->content_list        = NULL;
   manual_scan->dat_file            = NULL;
   manual_scan->list_size           = 0;
   manual_scan->list_index          = 0;
   manual_scan->status              = MANUAL_SCAN_BEGIN;
   manual_scan->fuzzy_archive_match = settings->bools.playlist_fuzzy_archive_match;
   manual_scan->use_old_format      = settings->bools.playlist_use_old_format;

   /* > Get current manual content scan configuration */
   manual_scan->task_config = (manual_content_scan_task_config_t*)
         calloc(1, sizeof(manual_content_scan_task_config_t));

   if (!manual_scan->task_config)
      goto error;

   if (!manual_content_scan_get_task_config(
            manual_scan->task_config,
            settings->paths.directory_playlist
            ))
   {
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_INVALID_CONFIG),
            1, 100, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      goto error;
   }

   /* Concurrent scanning of content to the same
    * playlist is not allowed */
   find_data.func     = task_manual_content_scan_finder;
   find_data.userdata = (void*)manual_scan->task_config->playlist_file;

   if (task_queue_find(&find_data))
      goto error;

   /* Create task */
   task = task_init();

   if (!task)
      goto error;

   /* > Get task title */
   strlcpy(
         task_title, msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_START),
         sizeof(task_title));
   strlcat(task_title, manual_scan->task_config->system_name,
         sizeof(task_title));

   /* > Configure task */
   task->handler                 = task_manual_content_scan_handler;
   task->state                   = manual_scan;
   task->title                   = strdup(task_title);
   task->alternative_look        = true;
   task->progress                = 0;
   task->callback                = cb_task_manual_content_scan_refresh_menu;

   /* > Push task */
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
   free_manual_content_scan_handle(manual_scan);
   manual_scan = NULL;

   return false;
}
