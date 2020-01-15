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
#include <file/archive_file.h>

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
   bool use_old_format;
   bool fuzzy_archive_match;
} pl_manager_handle_t;

/*********************/
/* Utility Functions */
/*********************/

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

static void pl_manager_write_playlist(
      playlist_t *playlist, const char *playlist_path, bool use_old_format)
{
   playlist_t *cached_playlist = playlist_get_cached();
   
   /* Sanity check */
   if (!playlist || string_is_empty(playlist_path))
      return;
   
   /* Write any changes to playlist file */
   playlist_write_file(playlist, use_old_format);
   
   /* If this is the currently cached playlist, then
    * it must be re-cached (otherwise changes will be
    * lost if the currently cached playlist is saved
    * to disk for any reason...) */
   if (cached_playlist)
   {
      if (string_is_equal(playlist_path, playlist_get_conf_path(cached_playlist)))
      {
         playlist_free_cached();
         playlist_init_cached(playlist_path, COLLECTION_SIZE);
      }
   }
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
            pl_manager->status = PL_MANAGER_ITERATE_ENTRY_RESET_CORE;
         }
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
               
               task_title[0] = '\0';
               
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
            
            task_title[0] = '\0';
            
            /* Save playlist changes to disk */
            pl_manager_write_playlist(
                  pl_manager->playlist,
                  pl_manager->playlist_path,
                  pl_manager->use_old_format);
            
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
   settings_t *settings            = config_get_ptr();
   retro_task_t *task              = task_init();
   pl_manager_handle_t *pl_manager = (pl_manager_handle_t*)calloc(1, sizeof(pl_manager_handle_t));
   
   playlist_name[0] = '\0';
   task_title[0]    = '\0';
   
   /* Sanity check */
   if (!task || !pl_manager || !settings)
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
   pl_manager->playlist_path       = strdup(playlist_path);
   pl_manager->playlist_name       = strdup(playlist_name);
   pl_manager->playlist            = NULL;
   pl_manager->list_size           = 0;
   pl_manager->list_index          = 0;
   pl_manager->status              = PL_MANAGER_BEGIN;
   pl_manager->use_old_format      = settings->bools.playlist_use_old_format;
   pl_manager->fuzzy_archive_match = false; /* Not relevant here */
   
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

/******************/
/* Clean Playlist */
/******************/

static bool pl_manager_content_exists(const char *path)
{
   /* Sanity check */
   if (string_is_empty(path))
      return false;
   
   /* If content is inside an archive, special
    * handling is required... */
   if (path_contains_compressed_file(path))
   {
      const char *delim                  = path_get_archive_delim(path);
      char archive_path[PATH_MAX_LENGTH] = {0};
      size_t len                         = 0;
      struct string_list *archive_list   = NULL;
      const char *content_file           = NULL;
      bool content_found                 = false;
      
      if (!delim)
         return false;
      
      /* Get path of 'parent' archive file */
      len = (size_t)(1 + delim - path);
      strlcpy(
            archive_path, path,
            (len < PATH_MAX_LENGTH ? len : PATH_MAX_LENGTH) * sizeof(char));
      
      /* Check if archive itself exists */
      if (!path_is_valid(archive_path))
         return false;
      
      /* Check if file exists inside archive */
      archive_list = file_archive_get_file_list(archive_path, NULL);
      
      if (!archive_list)
         return false;
      
      /* > Get playlist entry content file name
       *   (sans archive file path) */
      content_file = delim;
      content_file++;
      
      if (!string_is_empty(content_file))
      {
         size_t i;
         
         /* > Loop over archive file contents */
         for (i = 0; i < archive_list->size; i++)
         {
            const char *archive_file = archive_list->elems[i].data;
            
            if (string_is_empty(archive_file))
               continue;
            
            if (string_is_equal(content_file, archive_file))
            {
               content_found = true;
               break;
            }
         }
      }
      
      /* Clean up */
      string_list_free(archive_list);
      
      return content_found;
   }
   /* This is a 'normal' path - just check if
    * it's valid */
   else
      return path_is_valid(path);
}

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
      const char *core_path_basename = path_basename(core_path);
      core_info_list_t *core_info    = NULL;
      char core_display_name[PATH_MAX_LENGTH];
      size_t i;
      
      core_display_name[0] = '\0';
      
      if (string_is_empty(core_path_basename))
         goto reset_core;
      
      /* Final check - search core info */
      core_info_get_list(&core_info);
      
      if (core_info)
      {
         for (i = 0; i < core_info->count; i++)
         {
            const char *info_display_name = core_info->list[i].display_name;
            
            if (!string_is_equal(
                  path_basename(core_info->list[i].path), core_path_basename))
               continue;
            
            if (!string_is_empty(info_display_name))
               strlcpy(core_display_name, info_display_name, sizeof(core_display_name));
            
            break;
         }
      }
      
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
            if (!path_is_valid(pl_manager->playlist_path))
               goto task_finished;
            
            pl_manager->playlist = playlist_init(pl_manager->playlist_path, COLLECTION_SIZE);
            
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
            task_set_progress(task, (pl_manager->list_index * 50) / pl_manager->list_size);
            
            /* Get current entry */
            playlist_get_index(
                  pl_manager->playlist, pl_manager->list_index, &entry);
            
            if (entry)
            {
               /* Check whether playlist content exists on
                * the filesystem */
               if (!pl_manager_content_exists(entry->path))
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
         {
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
         }
         break;
      case PL_MANAGER_ITERATE_ENTRY_CHECK_DUPLICATE:
         {
            const struct playlist_entry *entry = NULL;
            bool entry_deleted                 = false;
            
            /* Update progress display */
            task_set_progress(task, 50 + (pl_manager->list_index * 50) / pl_manager->list_size);
            
            /* Get current entry */
            playlist_get_index(
                  pl_manager->playlist, pl_manager->list_index, &entry);
            
            if (entry)
            {
               size_t i;
               
               /* Loop over all subsequent entries, and check
                * whether content + core paths are the same */
               for (i = pl_manager->list_index + 1; i < pl_manager->list_size; i++)
               {
                  const struct playlist_entry *next_entry = NULL;
                  
                  /* Get next entry */
                  playlist_get_index(pl_manager->playlist, i, &next_entry);
                  
                  if (!next_entry)
                     continue;
                  
                  if (playlist_entries_are_equal(
                        entry, next_entry, pl_manager->fuzzy_archive_match))
                  {
                     /* Duplicate found - delete entry */
                     playlist_delete_index(pl_manager->playlist, pl_manager->list_index);
                     entry_deleted = true;
                     
                     /* Update list_size */
                     pl_manager->list_size = playlist_size(pl_manager->playlist);
                  }
               }
            }
            
            /* Increment entry index *if* current entry still
             * exists (i.e. if entry was deleted, current index
             * will already point to the *next* entry) */
            if (!entry_deleted)
               pl_manager->list_index++;
            
            if (pl_manager->list_index + 1 >= pl_manager->list_size)
               pl_manager->status = PL_MANAGER_END;
         }
         break;
      case PL_MANAGER_END:
         {
            char task_title[PATH_MAX_LENGTH];
            
            task_title[0] = '\0';
            
            /* Save playlist changes to disk */
            pl_manager_write_playlist(
                  pl_manager->playlist,
                  pl_manager->playlist_path,
                  pl_manager->use_old_format);
            
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
   
   free_pl_manager_handle(pl_manager);
}

static bool task_pl_manager_clean_playlist_finder(retro_task_t *task, void *user_data)
{
   pl_manager_handle_t *pl_manager = NULL;
   
   if (!task || !user_data)
      return false;
   
   if (task->handler != task_pl_manager_clean_playlist_handler)
      return false;
   
   pl_manager = (pl_manager_handle_t*)task->state;
   if (!pl_manager)
      return false;
   
   return string_is_equal((const char*)user_data, pl_manager->playlist_path);
}

bool task_push_pl_manager_clean_playlist(const char *playlist_path)
{
   task_finder_data_t find_data;
   char playlist_name[PATH_MAX_LENGTH];
   char task_title[PATH_MAX_LENGTH];
   settings_t *settings            = config_get_ptr();
   retro_task_t *task              = task_init();
   pl_manager_handle_t *pl_manager = (pl_manager_handle_t*)calloc(1, sizeof(pl_manager_handle_t));
   
   playlist_name[0] = '\0';
   task_title[0]    = '\0';
   
   /* Sanity check */
   if (!task || !pl_manager || !settings)
      goto error;
   
   if (string_is_empty(playlist_path))
      goto error;
   
   fill_pathname_base_noext(playlist_name, playlist_path, sizeof(playlist_name));
   
   if (string_is_empty(playlist_name))
      goto error;
   
   /* Concurrent management of the same playlist
    * is not allowed */
   find_data.func                = task_pl_manager_clean_playlist_finder;
   find_data.userdata            = (void*)playlist_path;
   
   if (task_queue_find(&find_data))
      goto error;
   
   /* Configure task */
   strlcpy(
         task_title, msg_hash_to_str(MSG_PLAYLIST_MANAGER_CLEANING_PLAYLIST),
         sizeof(task_title));
   strlcat(task_title, playlist_name, sizeof(task_title));
   
   task->handler                 = task_pl_manager_clean_playlist_handler;
   task->state                   = pl_manager;
   task->title                   = strdup(task_title);
   task->alternative_look        = true;
   task->progress                = 0;
   
   /* Configure handle */
   pl_manager->playlist_path       = strdup(playlist_path);
   pl_manager->playlist_name       = strdup(playlist_name);
   pl_manager->playlist            = NULL;
   pl_manager->list_size           = 0;
   pl_manager->list_index          = 0;
   pl_manager->status              = PL_MANAGER_BEGIN;
   pl_manager->use_old_format      = settings->bools.playlist_use_old_format;
   pl_manager->fuzzy_archive_match = settings->bools.playlist_fuzzy_archive_match;
   
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
