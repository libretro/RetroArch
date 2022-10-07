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
#include <formats/m3u_file.h>

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
   MANUAL_SCAN_ITERATE_CLEAN,
   MANUAL_SCAN_ITERATE_CONTENT,
   MANUAL_SCAN_ITERATE_M3U,
   MANUAL_SCAN_END
};

typedef struct manual_scan_handle
{
   manual_content_scan_task_config_t *task_config;
   playlist_t *playlist;
   struct string_list *file_exts_list;
   struct string_list *content_list;
   logiqx_dat_t *dat_file;
   struct string_list *m3u_list;
   playlist_config_t playlist_config; /* size_t alignment */
   size_t playlist_size;
   size_t playlist_index;
   size_t content_list_size;
   size_t content_list_index;
   size_t m3u_index;
   enum manual_scan_status status;
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

   if (manual_scan->file_exts_list)
   {
      string_list_free(manual_scan->file_exts_list);
      manual_scan->file_exts_list = NULL;
   }

   if (manual_scan->content_list)
   {
      string_list_free(manual_scan->content_list);
      manual_scan->content_list = NULL;
   }

   if (manual_scan->m3u_list)
   {
      string_list_free(manual_scan->m3u_list);
      manual_scan->m3u_list = NULL;
   }

   if (manual_scan->dat_file)
   {
      logiqx_dat_free(manual_scan->dat_file);
      manual_scan->dat_file = NULL;
   }

   free(manual_scan);
   manual_scan = NULL;
}

static void cb_task_manual_content_scan(
      retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   manual_scan_handle_t *manual_scan = NULL;
   playlist_t *cached_playlist       = playlist_get_cached();
#if defined(RARCH_INTERNAL) && defined(HAVE_MENU)
   menu_ctx_environment_t menu_environ;
   if (!task)
      goto end;
#else
   if (!task)
      return;
#endif

   manual_scan = (manual_scan_handle_t*)task->state;

   if (!manual_scan)
   {
#if defined(RARCH_INTERNAL) && defined(HAVE_MENU)
      goto end;
#else
      return;
#endif
   }

   /* If the manual content scan task has modified the
    * currently cached playlist, then it must be re-cached
    * (otherwise changes will be lost if the currently
    * cached playlist is saved to disk for any reason...) */
   if (cached_playlist)
   {
      if (string_is_equal(
            manual_scan->playlist_config.path,
            playlist_get_conf_path(cached_playlist)))
      {
         playlist_config_t playlist_config;

         /* Copy configuration of cached playlist
          * (could use manual_scan->playlist_config,
          * but doing it this way guarantees that
          * the cached playlist is preserved in
          * its original state) */
         if (playlist_config_copy(
               playlist_get_config(cached_playlist),
               &playlist_config))
         {
            playlist_free_cached();
            playlist_init_cached(&playlist_config);
         }
      }
   }

#if defined(RARCH_INTERNAL) && defined(HAVE_MENU)
end:
   /* When creating playlists, the playlist tabs of
    * any active menu driver must be refreshed */
   menu_environ.type = MENU_ENVIRON_RESET_HORIZONTAL_LIST;
   menu_environ.data = NULL;

   menu_driver_ctl(RARCH_MENU_CTL_ENVIRONMENT, &menu_environ);
#endif
}

static void task_manual_content_scan_free(retro_task_t *task)
{
   manual_scan_handle_t *manual_scan = NULL;

   if (!task)
      return;

   manual_scan = (manual_scan_handle_t*)task->state;

   free_manual_content_scan_handle(manual_scan);
}

static void task_manual_content_scan_handler(retro_task_t *task)
{
   manual_scan_handle_t *manual_scan = NULL;

   if (!task)
      goto task_finished;

   if (!(manual_scan = (manual_scan_handle_t*)task->state))
      goto task_finished;

   if (task_get_cancelled(task))
      goto task_finished;

   switch (manual_scan->status)
   {
      case MANUAL_SCAN_BEGIN:
         {
            /* Get allowed file extensions list */
            if (!string_is_empty(manual_scan->task_config->file_exts))
               manual_scan->file_exts_list = string_split(
                     manual_scan->task_config->file_exts, "|");

            /* Get content list */
            if (!(manual_scan->content_list 
                     = manual_content_scan_get_content_list(
                        manual_scan->task_config)))
            {
               runloop_msg_queue_push(
                     msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_INVALID_CONTENT),
                     1, 100, true,
                     NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               goto task_finished;
            }

            manual_scan->content_list_size = manual_scan->content_list->size;

            /* Load DAT file, if required */
            if (!string_is_empty(manual_scan->task_config->dat_file_path))
            {
               if (!(manual_scan->dat_file =
                     logiqx_dat_init(
                        manual_scan->task_config->dat_file_path)))
               {
                  runloop_msg_queue_push(
                        msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_DAT_FILE_LOAD_ERROR),
                        1, 100, true,
                        NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
                  goto task_finished;
               }
            }

            /* Open playlist */
            if (!(manual_scan->playlist =
                     playlist_init(&manual_scan->playlist_config)))
               goto task_finished;

            /* Reset playlist, if required */
            if (manual_scan->task_config->overwrite_playlist)
               playlist_clear(manual_scan->playlist);

            /* Get initial playlist size */
            manual_scan->playlist_size = 
               playlist_size(manual_scan->playlist);

            /* Set default core, if required */
            if (manual_scan->task_config->core_set)
            {
               playlist_set_default_core_path(manual_scan->playlist,
                     manual_scan->task_config->core_path);
               playlist_set_default_core_name(manual_scan->playlist,
                     manual_scan->task_config->core_name);
            }

            /* Record remaining scan parameters to enable
             * subsequent 'refresh playlist' operations */
            playlist_set_scan_content_dir(manual_scan->playlist,
                  manual_scan->task_config->content_dir);
            playlist_set_scan_file_exts(manual_scan->playlist,
                  manual_scan->task_config->file_exts_custom_set ?
                        manual_scan->task_config->file_exts : NULL);
            playlist_set_scan_dat_file_path(manual_scan->playlist,
                  manual_scan->task_config->dat_file_path);
            playlist_set_scan_search_recursively(manual_scan->playlist,
                  manual_scan->task_config->search_recursively);
            playlist_set_scan_search_archives(manual_scan->playlist,
                  manual_scan->task_config->search_archives);
            playlist_set_scan_filter_dat_content(manual_scan->playlist,
                  manual_scan->task_config->filter_dat_content);

            /* All good - can start iterating
             * > If playlist has content and 'validate
             *   entries' is enabled, go to clean-up phase
             * > Otherwise go straight to content scan phase */
            if (manual_scan->task_config->validate_entries &&
                (manual_scan->playlist_size > 0))
               manual_scan->status = MANUAL_SCAN_ITERATE_CLEAN;
            else
               manual_scan->status = MANUAL_SCAN_ITERATE_CONTENT;
         }
         break;
      case MANUAL_SCAN_ITERATE_CLEAN:
         {
            const struct playlist_entry *entry = NULL;
            bool delete_entry                  = false;

            /* Get current entry */
            playlist_get_index(manual_scan->playlist,
                  manual_scan->playlist_index, &entry);

            if (entry)
            {
               const char *entry_file     = NULL;
               const char *entry_file_ext = NULL;
               char task_title[PATH_MAX_LENGTH];

               /* Update progress display */
               task_free_title(task);

               strlcpy(task_title,
                     msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_PLAYLIST_CLEANUP),
                     sizeof(task_title));

               if (!string_is_empty(entry->path) &&
                   (entry_file = path_basename(entry->path)))
                  strlcat(task_title, entry_file, sizeof(task_title));

               task_set_title(task, strdup(task_title));
               task_set_progress(task, (manual_scan->playlist_index * 100) /
                     manual_scan->playlist_size);

               /* Check whether playlist content exists on
                * the filesystem */
               if (!playlist_content_path_is_valid(entry->path))
                  delete_entry = true;
               /* If file exists, check whether it has a
                * permitted file extension */
               else if (manual_scan->file_exts_list &&
                        (entry_file_ext = path_get_extension(entry->path)) &&
                        !string_list_find_elem_prefix(
                              manual_scan->file_exts_list,
                              ".", entry_file_ext))
                  delete_entry = true;

               if (delete_entry)
               {
                  /* Invalid content - delete entry */
                  playlist_delete_index(manual_scan->playlist,
                        manual_scan->playlist_index);

                  /* Update playlist_size */
                  manual_scan->playlist_size = playlist_size(manual_scan->playlist);
               }
            }

            /* Increment entry index *if* current entry still
             * exists (i.e. if entry was deleted, current index
             * will already point to the *next* entry) */
            if (!delete_entry)
               manual_scan->playlist_index++;

            if (manual_scan->playlist_index >=
                  manual_scan->playlist_size)
               manual_scan->status = MANUAL_SCAN_ITERATE_CONTENT;
         }
         break;
      case MANUAL_SCAN_ITERATE_CONTENT:
         {
            const char *content_path = manual_scan->content_list->elems[
                  manual_scan->content_list_index].data;
            int content_type         = manual_scan->content_list->elems[
                  manual_scan->content_list_index].attr.i;

            if (!string_is_empty(content_path))
            {
               char task_title[PATH_MAX_LENGTH];
               const char *content_file = path_basename(content_path);

               /* Update progress display */
               task_free_title(task);

               strlcpy(task_title,
                     msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_IN_PROGRESS),
                     sizeof(task_title));

               if (!string_is_empty(content_file))
                  strlcat(task_title, content_file, sizeof(task_title));

               task_set_title(task, strdup(task_title));
               task_set_progress(task,
                     (manual_scan->content_list_index * 100) /
                     manual_scan->content_list_size);

               /* Add content to playlist */
               manual_content_scan_add_content_to_playlist(
                     manual_scan->task_config, manual_scan->playlist,
                     content_path, content_type, manual_scan->dat_file);

               /* If this is an M3U file, add it to the
                * M3U list for later processing */
               if (m3u_file_is_m3u(content_path))
               {
                  union string_list_elem_attr attr;
                  attr.i = 0;
                  /* Note: If string_list_append() fails, there is
                   * really nothing we can do. The M3U file will
                   * just be ignored... */
                  string_list_append(
                        manual_scan->m3u_list, content_path, attr);
               }
            }

            /* Increment content index */
            manual_scan->content_list_index++;
            if (manual_scan->content_list_index >=
                  manual_scan->content_list_size)
            {
               /* Check whether we have any M3U files
                * to process */
               if (manual_scan->m3u_list->size > 0)
                  manual_scan->status = MANUAL_SCAN_ITERATE_M3U;
               else
                  manual_scan->status = MANUAL_SCAN_END;
            }
         }
         break;
      case MANUAL_SCAN_ITERATE_M3U:
         {
            const char *m3u_path = manual_scan->m3u_list->elems[
                  manual_scan->m3u_index].data;

            if (!string_is_empty(m3u_path))
            {
               char task_title[PATH_MAX_LENGTH];
               const char *m3u_name = path_basename_nocompression(m3u_path);
               m3u_file_t *m3u_file = NULL;

               /* Update progress display */
               task_free_title(task);

               strlcpy(task_title,
                     msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_M3U_CLEANUP),
                     sizeof(task_title));

               if (!string_is_empty(m3u_name))
                  strlcat(task_title, m3u_name, sizeof(task_title));

               task_set_title(task, strdup(task_title));
               task_set_progress(task, (manual_scan->m3u_index * 100) /
                     manual_scan->m3u_list->size);

               /* Load M3U file */
               if ((m3u_file = m3u_file_init(m3u_path)))
               {
                  size_t i;

                  /* Loop over M3U entries */
                  for (i = 0; i < m3u_file_get_size(m3u_file); i++)
                  {
                     m3u_file_entry_t *m3u_entry = NULL;

                     /* Delete any playlist items matching the
                      * content path of the M3U entry */
                     if (m3u_file_get_entry(m3u_file, i, &m3u_entry))
                        playlist_delete_by_path(
                              manual_scan->playlist, m3u_entry->full_path);
                  }

                  m3u_file_free(m3u_file);
               }
            }

            /* Increment M3U file index */
            manual_scan->m3u_index++;
            if (manual_scan->m3u_index >= manual_scan->m3u_list->size)
               manual_scan->status = MANUAL_SCAN_END;
         }
         break;
      case MANUAL_SCAN_END:
         {
            char task_title[PATH_MAX_LENGTH];

            /* Ensure playlist is alphabetically sorted
             * > Override user settings here */
            playlist_set_sort_mode(manual_scan->playlist, PLAYLIST_SORT_MODE_DEFAULT);
            playlist_qsort(manual_scan->playlist);

            /* Save playlist changes to disk */
            playlist_write_file(manual_scan->playlist);

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
}

static bool task_manual_content_scan_finder(retro_task_t *task, void *user_data)
{
   manual_scan_handle_t *manual_scan = NULL;

   if (!task || !user_data)
      return false;
   if (task->handler != task_manual_content_scan_handler)
      return false;
   if (!(manual_scan = (manual_scan_handle_t*)task->state))
      return false;
   return string_is_equal(
         (const char*)user_data, manual_scan->playlist_config.path);
}

bool task_push_manual_content_scan(
      const playlist_config_t *playlist_config,
      const char *playlist_directory)
{
   task_finder_data_t find_data;
   char task_title[PATH_MAX_LENGTH];
   retro_task_t *task                = NULL;
   manual_scan_handle_t *manual_scan = NULL;

   /* Sanity check */
   if (  !playlist_config
       || string_is_empty(playlist_directory))
      return false;

   if (!(manual_scan = (manual_scan_handle_t*)
         calloc(1, sizeof(manual_scan_handle_t))))
      return false;

   /* Configure handle */
   manual_scan->task_config         = NULL;
   manual_scan->playlist            = NULL;
   manual_scan->file_exts_list      = NULL;
   manual_scan->content_list        = NULL;
   manual_scan->dat_file            = NULL;
   manual_scan->playlist_size       = 0;
   manual_scan->playlist_index      = 0;
   manual_scan->content_list_size   = 0;
   manual_scan->content_list_index  = 0;
   manual_scan->status              = MANUAL_SCAN_BEGIN;
   manual_scan->m3u_index           = 0;
   manual_scan->m3u_list            = string_list_new();

   if (!manual_scan->m3u_list)
      goto error;

   /* > Get current manual content scan configuration */
   if (!(manual_scan->task_config = (manual_content_scan_task_config_t*)
         calloc(1, sizeof(manual_content_scan_task_config_t))))
      goto error;

   if ( !manual_content_scan_get_task_config(
         manual_scan->task_config, playlist_directory))
   {
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_MANUAL_CONTENT_SCAN_INVALID_CONFIG),
            1, 100, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      goto error;
   }

   /* > Cache playlist configuration */
   if (!playlist_config_copy(playlist_config,
         &manual_scan->playlist_config))
      goto error;

   playlist_config_set_path(
         &manual_scan->playlist_config,
         manual_scan->task_config->playlist_file);

   /* Concurrent scanning of content to the same
    * playlist is not allowed */
   find_data.func     = task_manual_content_scan_finder;
   find_data.userdata = (void*)manual_scan->playlist_config.path;

   if (task_queue_find(&find_data))
      goto error;

   /* Create task */
   if (!(task = task_init()))
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
   task->callback                = cb_task_manual_content_scan;
   task->cleanup                 = task_manual_content_scan_free;

   /* > Push task */
   task_queue_push(task);

   return true;

error:
   /* Clean up handle */
   free_manual_content_scan_handle(manual_scan);
   manual_scan = NULL;

   return false;
}
