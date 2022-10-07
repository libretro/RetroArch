/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2019 - Brad Parker
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

#include <string/stdstring.h>
#include <lists/string_list.h>
#include <file/file_path.h>
#include <formats/m3u_file.h>

#include "tasks_internal.h"

#include "../configuration.h"
#include "../msg_hash.h"
#include "../file_path_special.h"
#include "../playlist.h"
#include "../core_info.h"

enum pl_manager_status
{
   PL_MANAGER_BEGIN = 0,
   PL_MANAGER_ITERATE_ENTRY_RESET_CORE,
   PL_MANAGER_ITERATE_ENTRY_VALIDATE,
   PL_MANAGER_VALIDATE_END,
   PL_MANAGER_ITERATE_ENTRY_CHECK_DUPLICATE,
   PL_MANAGER_CHECK_DUPLICATE_END,
   PL_MANAGER_ITERATE_FETCH_M3U,
   PL_MANAGER_ITERATE_CLEAN_M3U,
   PL_MANAGER_END
};

typedef struct pl_manager_handle
{
   struct string_list *m3u_list;
   char *playlist_name;
   playlist_t *playlist;
   size_t list_size;
   size_t list_index;
   size_t m3u_index;
   playlist_config_t playlist_config; /* size_t alignment */
   enum pl_manager_status status;
} pl_manager_handle_t;

/*********************/
/* Utility Functions */
/*********************/

static void free_pl_manager_handle(pl_manager_handle_t *pl_manager)
{
   if (!pl_manager)
      return;
   
   if (pl_manager->m3u_list)
   {
      string_list_free(pl_manager->m3u_list);
      pl_manager->m3u_list = NULL;
   }
   
   if (!string_is_empty(pl_manager->playlist_name))
   {
      free(pl_manager->playlist_name);
      pl_manager->playlist_name = NULL;
   }
   
   if (pl_manager->playlist)
   {
      playlist_free(pl_manager->playlist);
      pl_manager->playlist = NULL;
   }
   
   free(pl_manager);
   pl_manager = NULL;
}

static void cb_task_pl_manager(
      retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   pl_manager_handle_t *pl_manager = NULL;
   playlist_t *cached_playlist     = playlist_get_cached();

   /* If no playlist is currently cached, no action
    * is required */
   if (!task || !cached_playlist)
      return;

   pl_manager = (pl_manager_handle_t*)task->state;

   if (!pl_manager)
      return;

   /* If the playlist manager task has modified the
    * currently cached playlist, then it must be re-cached
    * (otherwise changes will be lost if the currently
    * cached playlist is saved to disk for any reason...) */
   if (string_is_equal(
         pl_manager->playlist_config.path,
         playlist_get_conf_path(cached_playlist)))
   {
      playlist_config_t playlist_config;

      /* Copy configuration of cached playlist
       * (could use pl_manager->playlist_config,
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

static void task_pl_manager_free(retro_task_t *task)
{
   pl_manager_handle_t *pl_manager = NULL;

   if (!task)
      return;

   pl_manager = (pl_manager_handle_t*)task->state;

   free_pl_manager_handle(pl_manager);
}

/**************************/
/* Reset Associated Cores */
/**************************/

static void task_pl_manager_reset_cores_handler(retro_task_t *task)
{
   pl_manager_handle_t *pl_manager = NULL;
   
   if (!task)
      goto task_finished;
   
   pl_manager = (pl_manager_handle_t*)task->state;
   
   if (!pl_manager)
      goto task_finished;
   
   if (task_get_cancelled(task))
      goto task_finished;
   
   switch (pl_manager->status)
   {
      case PL_MANAGER_BEGIN:
         /* Load playlist */
         if (!path_is_valid(pl_manager->playlist_config.path))
            goto task_finished;

         pl_manager->playlist = playlist_init(&pl_manager->playlist_config);

         if (!pl_manager->playlist)
            goto task_finished;

         pl_manager->list_size = playlist_size(pl_manager->playlist);

         if (pl_manager->list_size < 1)
            goto task_finished;

         /* All good - can start iterating */
         pl_manager->status = PL_MANAGER_ITERATE_ENTRY_RESET_CORE;
         break;
      case PL_MANAGER_ITERATE_ENTRY_RESET_CORE:
         {
            const struct playlist_entry *entry = NULL;
            
            /* Get current entry */
            playlist_get_index(
                  pl_manager->playlist, pl_manager->list_index, &entry);
            
            if (entry)
            {
               struct playlist_entry update_entry = {0};
               char task_title[PATH_MAX_LENGTH];
               /* Update progress display */
               task_free_title(task);
               strlcpy(
                     task_title,
		     msg_hash_to_str(MSG_PLAYLIST_MANAGER_RESETTING_CORES),
                     sizeof(task_title));
               
               if (!string_is_empty(entry->label))
                  strlcat(task_title, entry->label, sizeof(task_title));
               else if (!string_is_empty(entry->path))
               {
                  char entry_name[PATH_MAX_LENGTH];
                  fill_pathname_base(entry_name, entry->path, sizeof(entry_name));
                  path_remove_extension(entry_name);
                  strlcat(task_title, entry_name, sizeof(task_title));
               }
               
               task_set_title(task, strdup(task_title));
               task_set_progress(task, (pl_manager->list_index * 100) / pl_manager->list_size);
               
               /* Reset core association
                * > The update function reads our entry as const,
                *   so these casts are safe */
               update_entry.core_path = (char*)"DETECT";
               update_entry.core_name = (char*)"DETECT";
               
               playlist_update(
                     pl_manager->playlist, pl_manager->list_index, &update_entry);
            }
            
            /* Increment entry index */
            pl_manager->list_index++;
            if (pl_manager->list_index >= pl_manager->list_size)
               pl_manager->status = PL_MANAGER_END;
         }
         break;
      case PL_MANAGER_END:
         {
            char task_title[PATH_MAX_LENGTH];
            /* Save playlist changes to disk */
            playlist_write_file(pl_manager->playlist);
            /* Update progress display */
            task_free_title(task);
            strlcpy(
                  task_title, msg_hash_to_str(MSG_PLAYLIST_MANAGER_CORES_RESET),
                  sizeof(task_title));
            strlcat(task_title, pl_manager->playlist_name, sizeof(task_title));
            
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

static bool task_pl_manager_reset_cores_finder(
      retro_task_t *task, void *user_data)
{
   pl_manager_handle_t *pl_manager = NULL;
   
   if (!task || !user_data)
      return false;
   
   if (task->handler != task_pl_manager_reset_cores_handler)
      return false;
   
   if (!(pl_manager = (pl_manager_handle_t*)task->state))
      return false;
   
   return string_is_equal((const char*)user_data,
         pl_manager->playlist_config.path);
}

bool task_push_pl_manager_reset_cores(const playlist_config_t *playlist_config)
{
   task_finder_data_t find_data;
   char playlist_name[PATH_MAX_LENGTH];
   char task_title[PATH_MAX_LENGTH];
   retro_task_t *task              = task_init();
   pl_manager_handle_t *pl_manager = (pl_manager_handle_t*)
      calloc(1, sizeof(pl_manager_handle_t));
   /* Sanity check */
   if (!playlist_config || !task || !pl_manager)
      goto error;
   if (string_is_empty(playlist_config->path))
      goto error;
   
   fill_pathname_base(playlist_name,
         playlist_config->path, sizeof(playlist_name));
   path_remove_extension(playlist_name);
   
   if (string_is_empty(playlist_name))
      goto error;
   
   /* Concurrent management of the same playlist
    * is not allowed */
   find_data.func                = task_pl_manager_reset_cores_finder;
   find_data.userdata            = (void*)playlist_config->path;
   
   if (task_queue_find(&find_data))
      goto error;
   
   /* Configure handle */
   if (!playlist_config_copy(playlist_config, &pl_manager->playlist_config))
      goto error;
   
   pl_manager->playlist_name       = strdup(playlist_name);
   pl_manager->playlist            = NULL;
   pl_manager->list_size           = 0;
   pl_manager->list_index          = 0;
   pl_manager->m3u_list            = NULL;
   pl_manager->m3u_index           = 0;
   pl_manager->status              = PL_MANAGER_BEGIN;
   
   /* Configure task */
   strlcpy(
         task_title,
	 msg_hash_to_str(MSG_PLAYLIST_MANAGER_RESETTING_CORES),
         sizeof(task_title));
   strlcat(task_title, playlist_name, sizeof(task_title));
   
   task->handler                 = task_pl_manager_reset_cores_handler;
   task->state                   = pl_manager;
   task->title                   = strdup(task_title);
   task->alternative_look        = true;
   task->progress                = 0;
   task->callback                = cb_task_pl_manager;
   task->cleanup                 = task_pl_manager_free;
   
   task_queue_push(task);
   
   return true;
   
error:
   
   if (task)
   {
      free(task);
      task = NULL;
   }
   
   free_pl_manager_handle(pl_manager);
   pl_manager = NULL;
   
   return false;
}

/******************/
/* Clean Playlist */
/******************/

static void pl_manager_validate_core_association(
      playlist_t *playlist, size_t entry_index,
      const char *core_path, const char *core_name)
{
   struct playlist_entry update_entry = {0};
   
   /* Sanity check */
   if (!playlist)
      return;
   
   if (entry_index >= playlist_size(playlist))
      return;
   
   if (string_is_empty(core_path))
      goto reset_core;
   
   /* Handle 'DETECT' entries */
   if (string_is_equal(core_path, "DETECT"))
   {
      if (!string_is_equal(core_name, "DETECT"))
         goto reset_core;
   }
   /* Handle 'builtin' entries */
   else if (string_is_equal(core_path, "builtin"))
   {
      if (string_is_empty(core_name))
         goto reset_core;
   }
   /* Handle file path entries */
   else if (!path_is_valid(core_path))
      goto reset_core;
   else
   {
      char core_display_name[PATH_MAX_LENGTH];
      core_info_t *core_info = NULL;
      
      /* Search core info */
      if (core_info_find(core_path, &core_info) &&
          !string_is_empty(core_info->display_name))
         strlcpy(core_display_name, core_info->display_name,
               sizeof(core_display_name));
      else
         core_display_name[0] = '\0';
      
      /* If core_display_name string is empty, it means the
       * core wasn't found -> reset association */
      if (string_is_empty(core_display_name))
         goto reset_core;
      
      /* ...Otherwise, check that playlist entry
       * core name is correct */
      if (!string_is_equal(core_name, core_display_name))
      {
         update_entry.core_name = core_display_name;
         playlist_update(playlist, entry_index, &update_entry);
      }
   }
   
   return;
   
reset_core:
   /* The update function reads our entry as const,
    * so these casts are safe */
   update_entry.core_path = (char*)"DETECT";
   update_entry.core_name = (char*)"DETECT";
   
   playlist_update(playlist, entry_index, &update_entry);
}

static void task_pl_manager_clean_playlist_handler(retro_task_t *task)
{
   pl_manager_handle_t *pl_manager = NULL;
   
   if (!task)
      goto task_finished;
   
   pl_manager = (pl_manager_handle_t*)task->state;
   
   if (!pl_manager)
      goto task_finished;
   
   if (task_get_cancelled(task))
      goto task_finished;
   
   switch (pl_manager->status)
   {
      case PL_MANAGER_BEGIN:
         {
            /* Load playlist */
            if (!path_is_valid(pl_manager->playlist_config.path))
               goto task_finished;
            
            pl_manager->playlist = playlist_init(&pl_manager->playlist_config);
            
            if (!pl_manager->playlist)
               goto task_finished;
            
            pl_manager->list_size = playlist_size(pl_manager->playlist);
            
            if (pl_manager->list_size < 1)
               goto task_finished;
            
            /* All good - can start iterating */
            pl_manager->status = PL_MANAGER_ITERATE_ENTRY_VALIDATE;
         }
         break;
      case PL_MANAGER_ITERATE_ENTRY_VALIDATE:
         {
            const struct playlist_entry *entry = NULL;
            bool entry_deleted                 = false;
            
            /* Update progress display */
            task_set_progress(task, (pl_manager->list_index * 100) / pl_manager->list_size);
            
            /* Get current entry */
            playlist_get_index(
                  pl_manager->playlist, pl_manager->list_index, &entry);
            
            if (entry)
            {
               /* Check whether playlist content exists on
                * the filesystem */
               if (!playlist_content_path_is_valid(entry->path))
               {
                  /* Invalid content - delete entry */
                  playlist_delete_index(pl_manager->playlist, pl_manager->list_index);
                  entry_deleted = true;
                  
                  /* Update list_size */
                  pl_manager->list_size = playlist_size(pl_manager->playlist);
               }
               /* Content is valid - check if core is valid */
               else
                  pl_manager_validate_core_association(
                        pl_manager->playlist, pl_manager->list_index,
                        entry->core_path, entry->core_name);
            }
            
            /* Increment entry index *if* current entry still
             * exists (i.e. if entry was deleted, current index
             * will already point to the *next* entry) */
            if (!entry_deleted)
               pl_manager->list_index++;
            
            if (pl_manager->list_index >= pl_manager->list_size)
               pl_manager->status = PL_MANAGER_VALIDATE_END;
         }
         break;
      case PL_MANAGER_VALIDATE_END:
         /* Sanity check - if all (or all but one)
          * playlist entries were removed during the
          * 'validate' phase, we can stop now */
         if (pl_manager->list_size < 2)
         {
            pl_manager->status = PL_MANAGER_END;
            break;
         }

         /* ...otherwise, reset index counter and
          * start the duplicates check */
         pl_manager->list_index = 0;
         pl_manager->status = PL_MANAGER_ITERATE_ENTRY_CHECK_DUPLICATE;
         break;
      case PL_MANAGER_ITERATE_ENTRY_CHECK_DUPLICATE:
         {
            bool entry_deleted = false;
            size_t i;
            
            /* Update progress display */
            task_set_progress(task, (pl_manager->list_index * 100) / pl_manager->list_size);
            
            /* Check whether the content + core paths of the
             * current entry match those of any subsequent
             * entry */
            for (i = pl_manager->list_index + 1; i < pl_manager->list_size; i++)
            {
               if (playlist_index_entries_are_equal(pl_manager->playlist,
                     pl_manager->list_index, i))
               {
                  /* Duplicate found - delete entry */
                  playlist_delete_index(pl_manager->playlist, pl_manager->list_index);
                  entry_deleted = true;
                  
                  /* Update list_size */
                  pl_manager->list_size = playlist_size(pl_manager->playlist);
                  break;
               }
            }
            
            /* Increment entry index *if* current entry still
             * exists (i.e. if entry was deleted, current index
             * will already point to the *next* entry) */
            if (!entry_deleted)
               pl_manager->list_index++;
            
            if (pl_manager->list_index + 1 >= pl_manager->list_size)
               pl_manager->status = PL_MANAGER_CHECK_DUPLICATE_END;
         }
         break;
      case PL_MANAGER_CHECK_DUPLICATE_END:
         /* Sanity check - if all (or all but one)
          * playlist entries were removed during the
          * 'check duplicate' phase, we can stop now */
         if (pl_manager->list_size < 2)
         {
            pl_manager->status = PL_MANAGER_END;
            break;
         }

         /* ...otherwise, reset index counter and
          * start building the M3U file list */
         pl_manager->list_index = 0;
         pl_manager->status = PL_MANAGER_ITERATE_FETCH_M3U;
         break;
      case PL_MANAGER_ITERATE_FETCH_M3U:
         {
            const struct playlist_entry *entry = NULL;
            
            /* Update progress display */
            task_set_progress(task, (pl_manager->list_index * 100) / pl_manager->list_size);
            
            /* Get current entry */
            playlist_get_index(
                  pl_manager->playlist, pl_manager->list_index, &entry);
            
            if (entry)
            {
               /* If this is an M3U file, add it to the
                * M3U list for later processing */
               if (m3u_file_is_m3u(entry->path))
               {
                  union string_list_elem_attr attr;
                  attr.i = 0;
                  /* Note: If string_list_append() fails, there is
                   * really nothing we can do. The M3U file will
                   * just be ignored... */
                  string_list_append(
                        pl_manager->m3u_list, entry->path, attr);
               }
            }
            
            /* Increment entry index */
            pl_manager->list_index++;
            
            if (pl_manager->list_index >= pl_manager->list_size)
            {
               /* Check whether we have any M3U files
                * to process */
               if (pl_manager->m3u_list->size > 0)
                  pl_manager->status = PL_MANAGER_ITERATE_CLEAN_M3U;
               else
                  pl_manager->status = PL_MANAGER_END;
            }
         }
         break;
      case PL_MANAGER_ITERATE_CLEAN_M3U:
         {
            const char *m3u_path =
                  pl_manager->m3u_list->elems[pl_manager->m3u_index].data;
            
            if (!string_is_empty(m3u_path))
            {
               m3u_file_t *m3u_file = NULL;
               
               /* Update progress display */
               task_set_progress(task, (pl_manager->m3u_index * 100) / pl_manager->m3u_list->size);
               
               /* Load M3U file */
               m3u_file = m3u_file_init(m3u_path);
               
               if (m3u_file)
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
                              pl_manager->playlist, m3u_entry->full_path);
                  }
                  
                  m3u_file_free(m3u_file);
               }
            }
            
            /* Increment M3U file index */
            pl_manager->m3u_index++;
            if (pl_manager->m3u_index >= pl_manager->m3u_list->size)
               pl_manager->status = PL_MANAGER_END;
         }
         break;
      case PL_MANAGER_END:
         {
            char task_title[PATH_MAX_LENGTH];
            /* Save playlist changes to disk */
            playlist_write_file(pl_manager->playlist);
            /* Update progress display */
            task_free_title(task);
            strlcpy(
                  task_title, msg_hash_to_str(MSG_PLAYLIST_MANAGER_PLAYLIST_CLEANED),
                  sizeof(task_title));
            strlcat(task_title, pl_manager->playlist_name, sizeof(task_title));
            
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

static bool task_pl_manager_clean_playlist_finder(
      retro_task_t *task, void *user_data)
{
   pl_manager_handle_t *pl_manager = NULL;
   
   if (!task || !user_data)
      return false;
   
   if (task->handler != task_pl_manager_clean_playlist_handler)
      return false;
   
   pl_manager = (pl_manager_handle_t*)task->state;
   if (!pl_manager)
      return false;
   
   return string_is_equal((const char*)user_data,
         pl_manager->playlist_config.path);
}

bool task_push_pl_manager_clean_playlist(
      const playlist_config_t *playlist_config)
{
   task_finder_data_t find_data;
   char playlist_name[PATH_MAX_LENGTH];
   char task_title[PATH_MAX_LENGTH];
   retro_task_t *task              = task_init();
   pl_manager_handle_t *pl_manager = (pl_manager_handle_t*)
      calloc(1, sizeof(pl_manager_handle_t));
   /* Sanity check */
   if (!playlist_config || !task || !pl_manager)
      goto error;
   if (string_is_empty(playlist_config->path))
      goto error;
   
   fill_pathname_base(playlist_name,
         playlist_config->path, sizeof(playlist_name));
   path_remove_extension(playlist_name);
   
   if (string_is_empty(playlist_name))
      goto error;
   
   /* Concurrent management of the same playlist
    * is not allowed */
   find_data.func                = task_pl_manager_clean_playlist_finder;
   find_data.userdata            = (void*)playlist_config->path;
   
   if (task_queue_find(&find_data))
      goto error;
   
   /* Configure handle */
   if (!playlist_config_copy(playlist_config, &pl_manager->playlist_config))
      goto error;
   
   pl_manager->playlist_name       = strdup(playlist_name);
   pl_manager->playlist            = NULL;
   pl_manager->list_size           = 0;
   pl_manager->list_index          = 0;
   pl_manager->m3u_list            = string_list_new();
   pl_manager->m3u_index           = 0;
   pl_manager->status              = PL_MANAGER_BEGIN;
   
   if (!pl_manager->m3u_list)
      goto error;
   
   /* Configure task */
   strlcpy(
         task_title,
	 msg_hash_to_str(MSG_PLAYLIST_MANAGER_CLEANING_PLAYLIST),
         sizeof(task_title));
   strlcat(task_title, playlist_name, sizeof(task_title));
   
   task->handler                 = task_pl_manager_clean_playlist_handler;
   task->state                   = pl_manager;
   task->title                   = strdup(task_title);
   task->alternative_look        = true;
   task->progress                = 0;
   task->callback                = cb_task_pl_manager;
   task->cleanup                 = task_pl_manager_free;
   
   task_queue_push(task);
   
   return true;
   
error:
   
   if (task)
   {
      free(task);
      task = NULL;
   }
   
   free_pl_manager_handle(pl_manager);
   pl_manager = NULL;
   
   return false;
}
