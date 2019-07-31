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
#include <file/file_path.h>

#include "tasks_internal.h"

#include "../msg_hash.h"
#include "../file_path_special.h"
#include "../playlist.h"

enum pl_manager_status
{
   PL_MANAGER_BEGIN = 0,
   PL_MANAGER_ITERATE_ENTRY,
   PL_MANAGER_END
};

typedef struct pl_manager_handle
{
   char *playlist_path;
   char *playlist_name;
   playlist_t *playlist;
   size_t list_size;
   size_t list_index;
   enum pl_manager_status status;
} pl_manager_handle_t;

/**************************/
/* Reset Associated Cores */
/**************************/

static void free_pl_manager_handle(pl_manager_handle_t *pl_manager)
{
   if (!pl_manager)
      return;
   
   if (!string_is_empty(pl_manager->playlist_path))
   {
      free(pl_manager->playlist_path);
      pl_manager->playlist_path = NULL;
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
         {
            /* Load playlist */
            if (!path_is_valid(pl_manager->playlist_path))
               goto task_finished;
            
            pl_manager->playlist = playlist_init(pl_manager->playlist_path, COLLECTION_SIZE);
            
            if (!pl_manager->playlist)
               goto task_finished;
            
            pl_manager->list_size = playlist_size(pl_manager->playlist);
            
            if (pl_manager->list_size < 1)
               goto task_finished;
            
            /* All good - can start iterating */
            pl_manager->status = PL_MANAGER_ITERATE_ENTRY;
         }
         break;
      case PL_MANAGER_ITERATE_ENTRY:
         {
            const struct playlist_entry *entry = NULL;
            
            /* Get current entry */
            playlist_get_index(
                  pl_manager->playlist, pl_manager->list_index, &entry);
            
            if (entry)
            {
               struct playlist_entry update_entry = {0};
               char task_title[PATH_MAX_LENGTH];
               char detect_string[PATH_MAX_LENGTH];
               
               task_title[0]    = '\0';
               detect_string[0] = '\0';
               
               /* Update progress display */
               task_free_title(task);
               
               strlcpy(
                     task_title, msg_hash_to_str(MSG_PLAYLIST_MANAGER_RESETTING_CORES),
                     sizeof(task_title));
               
               if (!string_is_empty(entry->label))
                  strlcat(task_title, entry->label, sizeof(task_title));
               else if (!string_is_empty(entry->path))
               {
                  char entry_name[PATH_MAX_LENGTH];
                  entry_name[0] = '\0';
                  
                  fill_pathname_base_noext(entry_name, entry->path, sizeof(entry_name));
                  strlcat(task_title, entry_name, sizeof(task_title));
               }
               
               task_set_title(task, strdup(task_title));
               task_set_progress(task, (pl_manager->list_index * 100) / pl_manager->list_size);
               
               /* Reset core association */
               strlcpy(detect_string, file_path_str(FILE_PATH_DETECT), sizeof(detect_string));
               
               update_entry.core_path = detect_string;
               update_entry.core_name = detect_string;
               
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
            playlist_t *cached_playlist = playlist_get_cached();
            char task_title[PATH_MAX_LENGTH];
            
            task_title[0] = '\0';
            
            /* Save playlist changes to disk */
            playlist_write_file(pl_manager->playlist);
            
            /* If this is the currently cached playlist, then
             * it must be re-cached (otherwise changes will be
             * lost if the currently cached playlist is saved
             * to disk for any reason...) */
            if (cached_playlist)
            {
               if (string_is_equal(pl_manager->playlist_path, playlist_get_conf_path(cached_playlist)))
               {
                  playlist_free_cached();
                  playlist_init_cached(pl_manager->playlist_path, COLLECTION_SIZE);
               }
            }
            
            /* Update progress display */
            task_free_title(task);
            
            strlcpy(
                  task_title, msg_hash_to_str(MSG_PLAYLIST_MANAGER_CORES_RESET),
                  sizeof(task_title));
            strlcat(task_title, pl_manager->playlist_name, sizeof(task_title));
            
            task_set_title(task, strdup(task_title));
            task_set_progress(task, 100);
            
            goto task_finished;
         }
         break;
      default:
         task_set_progress(task, 100);
         goto task_finished;
         break;
   }
   
   return;
   
task_finished:
   
   if (task)
      task_set_finished(task, true);
   
   free_pl_manager_handle(pl_manager);
}

static bool task_pl_manager_reset_cores_finder(retro_task_t *task, void *user_data)
{
   pl_manager_handle_t *pl_manager = NULL;
   
   if (!task || !user_data)
      return false;
   
   if (task->handler != task_pl_manager_reset_cores_handler)
      return false;
   
   pl_manager = (pl_manager_handle_t*)task->state;
   if (!pl_manager)
      return false;
   
   return string_is_equal((const char*)user_data, pl_manager->playlist_path);
}

bool task_push_pl_manager_reset_cores(const char *playlist_path)
{
   task_finder_data_t find_data;
   char playlist_name[PATH_MAX_LENGTH];
   char task_title[PATH_MAX_LENGTH];
   retro_task_t *task              = task_init();
   pl_manager_handle_t *pl_manager = (pl_manager_handle_t*)calloc(1, sizeof(pl_manager_handle_t));
   
   playlist_name[0] = '\0';
   task_title[0]    = '\0';
   
   /* Sanity check */
   if (!task || !pl_manager)
      goto error;
   
   if (string_is_empty(playlist_path))
      goto error;
   
   fill_pathname_base_noext(playlist_name, playlist_path, sizeof(playlist_name));
   
   if (string_is_empty(playlist_name))
      goto error;
   
   /* Concurrent management of the same playlist
    * is not allowed */
   find_data.func                = task_pl_manager_reset_cores_finder;
   find_data.userdata            = (void*)playlist_path;
   
   if (task_queue_find(&find_data))
      goto error;
   
   /* Configure task */
   strlcpy(
         task_title, msg_hash_to_str(MSG_PLAYLIST_MANAGER_RESETTING_CORES),
         sizeof(task_title));
   strlcat(task_title, playlist_name, sizeof(task_title));
   
   task->handler                 = task_pl_manager_reset_cores_handler;
   task->state                   = pl_manager;
   task->title                   = strdup(task_title);
   task->alternative_look        = true;
   task->progress                = 0;
   
   /* Configure handle */
   pl_manager->playlist_path     = strdup(playlist_path);
   pl_manager->playlist_name     = strdup(playlist_name);
   pl_manager->playlist          = NULL;
   pl_manager->list_size         = 0;
   pl_manager->list_index        = 0;
   pl_manager->status            = PL_MANAGER_BEGIN;
   
   task_queue_push(task);
   
   return true;
   
error:
   
   if (task)
   {
      free(task);
      task = NULL;
   }
   
   if (pl_manager)
   {
      free(pl_manager);
      pl_manager = NULL;
   }
   
   return false;
}
