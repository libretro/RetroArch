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
#include <streams/file_stream.h>
#include <net/net_http.h>

#include "tasks_internal.h"
#include "task_file_transfer.h"

#include "../file_path_special.h"
#include "../playlist.h"

#ifdef RARCH_INTERNAL
#ifdef HAVE_MENU
#include "../menu/menu_thumbnail_path.h"
#include "../menu/menu_cbs.h"
#include "../menu/menu_driver.h"
#endif
#endif

#ifndef COLLECTION_SIZE
#define COLLECTION_SIZE 99999
#endif

enum pl_thumb_status
{
   PL_THUMB_BEGIN = 0,
   PL_THUMB_ITERATE_ENTRY,
   PL_THUMB_ITERATE_TYPE,
   PL_THUMB_END
};

typedef struct pl_thumb_handle
{
   char *system;
   char *playlist_path;
   char *dir_thumbnails;
   playlist_t *playlist;
   menu_thumbnail_path_data_t *thumbnail_path_data;
   retro_task_t *http_task;
   size_t list_size;
   size_t list_index;
   unsigned type_idx;
   enum pl_thumb_status status;
} pl_thumb_handle_t;

/*********************/
/* Utility Functions */
/*********************/

/* Fetches local and remote paths for current thumbnail
 * of current type */
static bool get_thumbnail_paths(
   pl_thumb_handle_t *pl_thumb,
   char *path, size_t path_size,
   char *url, size_t url_size)
{
   const char *system      = NULL;
   const char *db_name     = NULL;
   const char *img_name    = NULL;
   const char *sub_dir     = NULL;
   const char *system_name = NULL;
   char content_dir[PATH_MAX_LENGTH];
   char raw_url[2048];
   char tmp_buf[PATH_MAX_LENGTH];
   
   content_dir[0] = '\0';
   raw_url[0]     = '\0';
   tmp_buf[0]     = '\0';
   
   if (!pl_thumb->thumbnail_path_data)
      return false;
   
   if (string_is_empty(pl_thumb->dir_thumbnails))
      return false;
   
   /* Extract required strings */
   menu_thumbnail_get_system(pl_thumb->thumbnail_path_data, &system);
   menu_thumbnail_get_db_name(pl_thumb->thumbnail_path_data, &db_name);
   if (!menu_thumbnail_get_img_name(pl_thumb->thumbnail_path_data, &img_name))
      return false;
   if (!menu_thumbnail_get_sub_directory(pl_thumb->type_idx, &sub_dir))
      return false;
   
   /* Dermine system name */
   if (string_is_empty(db_name))
   {
      if (string_is_empty(system))
         return false;
      
      /* If this is a content history or favorites playlist
       * then the current 'path_data->system' string is
       * meaningless. In this case, we fall back to the
       * content directory name */
      if (string_is_equal(system, "history") ||
          string_is_equal(system, "favorites"))
      {
         if (!menu_thumbnail_get_content_dir(
               pl_thumb->thumbnail_path_data, content_dir, sizeof(content_dir)))
            return false;
         
         system_name = content_dir;
      }
      else
         system_name = system;
   }
   else
      system_name = db_name;
   
   /* Generate local path */
   fill_pathname_join(path, pl_thumb->dir_thumbnails,
         system_name, path_size);
   fill_pathname_join(tmp_buf, path, sub_dir, sizeof(tmp_buf));
   fill_pathname_join(path, tmp_buf, img_name, path_size);
   
   if (string_is_empty(path))
      return false;
   
   /* Generate remote path */
   strlcpy(raw_url, file_path_str(FILE_PATH_CORE_THUMBNAILS_URL), sizeof(raw_url));
   strlcat(raw_url, "/", sizeof(raw_url));
   strlcat(raw_url, system_name, sizeof(raw_url));
   strlcat(raw_url, "/", sizeof(raw_url));
   strlcat(raw_url, sub_dir, sizeof(raw_url));
   strlcat(raw_url, "/", sizeof(raw_url));
   strlcat(raw_url, img_name, sizeof(raw_url));
   
   if (string_is_empty(raw_url))
      return false;
   
   net_http_urlencode_full(url, raw_url, url_size);
   
   if (string_is_empty(url))
      return false;
   
   return true;
}

/* Download thumbnail of the current type for the current
 * playlist entry */
static void download_pl_thumbnail(pl_thumb_handle_t *pl_thumb, bool overwrite)
{
   char path[PATH_MAX_LENGTH];
   char url[2048];
   
   path[0] = '\0';
   url[0] = '\0';
   
   /* Check if paths are valid */
   if (get_thumbnail_paths(pl_thumb, path, sizeof(path), url, sizeof(url)))
   {
      /* Only download missing thumbnails */
      if (!filestream_exists(path) || overwrite)
      {
         file_transfer_t *transf = (file_transfer_t*)calloc(1, sizeof(file_transfer_t));
         if (!transf)
            return; /* If this happens then everything is broken anyway... */
         
         /* Initialise file transfer */
         transf->enum_idx = MENU_ENUM_LABEL_CB_SINGLE_THUMBNAIL;
         strlcpy(transf->path, path, sizeof(transf->path));
         
         /* Note: We don't actually care if this fails since that
          * just means the file is missing from the server, so it's
          * not something we can handle here... */
         pl_thumb->http_task = (retro_task_t*)task_push_http_transfer(url, true, NULL, cb_generic_download, transf);
      }
   }
}

static void free_pl_thumb_handle(pl_thumb_handle_t *pl_thumb, bool free_playlist)
{
   if (!pl_thumb)
      return;

   if (!string_is_empty(pl_thumb->system))
   {
      free(pl_thumb->system);
      pl_thumb->system = NULL;
   }

   if (!string_is_empty(pl_thumb->playlist_path))
   {
      free(pl_thumb->playlist_path);
      pl_thumb->playlist_path = NULL;
   }

   if (!string_is_empty(pl_thumb->dir_thumbnails))
   {
      free(pl_thumb->dir_thumbnails);
      pl_thumb->dir_thumbnails = NULL;
   }

   if (pl_thumb->playlist && free_playlist)
   {
      playlist_free(pl_thumb->playlist);
      pl_thumb->playlist = NULL;
   }

   if (pl_thumb->thumbnail_path_data)
   {
      free(pl_thumb->thumbnail_path_data);
      pl_thumb->thumbnail_path_data = NULL;
   }

   free(pl_thumb);
   pl_thumb = NULL;
}

/* Callback: Refresh menu thumbnail display once
 * download is complete */
static void cb_task_pl_thumbnail_refresh_menu(
      retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
#if defined(RARCH_INTERNAL) && defined(HAVE_MENU)
   menu_driver_ctl(RARCH_MENU_CTL_REFRESH_THUMBNAIL_IMAGE, NULL);
#endif
}

/*******************************/
/* Playlist Thumbnail Download */
/*******************************/

static void task_pl_thumbnail_download_handler(retro_task_t *task)
{
   pl_thumb_handle_t *pl_thumb = NULL;
   
   if (!task)
      goto task_finished;
   
   pl_thumb = (pl_thumb_handle_t*)task->state;
   
   if (!pl_thumb)
      goto task_finished;
   
   if (task_get_cancelled(task))
      goto task_finished;
   
   switch (pl_thumb->status)
   {
      case PL_THUMB_BEGIN:
         {
            /* Load playlist */
            if (!filestream_exists(pl_thumb->playlist_path))
               goto task_finished;
            
            pl_thumb->playlist = playlist_init(pl_thumb->playlist_path, COLLECTION_SIZE);
            
            if (!pl_thumb->playlist)
               goto task_finished;
            
            pl_thumb->list_size = playlist_size(pl_thumb->playlist);
            
            if (pl_thumb->list_size < 1)
               goto task_finished;
            
            /* Initialise thumbnail path data */
            pl_thumb->thumbnail_path_data = menu_thumbnail_path_init();
            
            if (!pl_thumb->thumbnail_path_data)
               goto task_finished;
            
            if (!menu_thumbnail_set_system(pl_thumb->thumbnail_path_data, pl_thumb->system))
               goto task_finished;
            
            /* All good - can start iterating */
            pl_thumb->status = PL_THUMB_ITERATE_ENTRY;
         }
         break;
      case PL_THUMB_ITERATE_ENTRY:
         {
            /* Set current thumbnail content */
            if (menu_thumbnail_set_content_playlist(
                  pl_thumb->thumbnail_path_data, pl_thumb->playlist, pl_thumb->list_index))
            {
               const char *label = NULL;
               
               /* Update progress display */
               task_free_title(task);
               if (menu_thumbnail_get_label(pl_thumb->thumbnail_path_data, &label))
                  task_set_title(task, strdup(label));
               else
                  task_set_title(task, strdup(""));
               task_set_progress(task, (pl_thumb->list_index * 100) / pl_thumb->list_size);
               
               /* Start iterating over thumbnail type */
               pl_thumb->type_idx = 1;
               pl_thumb->status = PL_THUMB_ITERATE_TYPE;
            }
            else
            {
               /* Current playlist entry is broken - advance to
                * the next one */
               pl_thumb->list_index++;
               if (pl_thumb->list_index >= pl_thumb->list_size)
                  pl_thumb->status = PL_THUMB_END;
            }
         }
         break;
      case PL_THUMB_ITERATE_TYPE:
         {
            /* Ensure that we only enqueue one transfer
             * at a time... */
            if (pl_thumb->http_task)
            {
               if (task_get_finished(pl_thumb->http_task))
                  pl_thumb->http_task = NULL;
               else
                  break;
            }
            
            /* Check whether all thumbnail types have been processed */
            if (pl_thumb->type_idx > 3)
            {
               /* Time to move on to the next entry */
               pl_thumb->list_index++;
               if (pl_thumb->list_index < pl_thumb->list_size)
                  pl_thumb->status = PL_THUMB_ITERATE_ENTRY;
               else
                  pl_thumb->status = PL_THUMB_END;
               break;
            }
            
            /* Download current thumbnail */
            if (pl_thumb)
               download_pl_thumbnail(pl_thumb, false);
            
            /* Increment thumbnail type */
            pl_thumb->type_idx++;
         }
         break;
      case PL_THUMB_END:
      default:
         task_set_progress(task, 100);
         goto task_finished;
         break;
   }
   
   return;
   
task_finished:
   
   if (task)
      task_set_finished(task, true);
   
   free_pl_thumb_handle(pl_thumb, true);
}

bool task_push_pl_thumbnail_download(
      const char *system, const char *playlist_path)
{
   retro_task_t *task            = task_init();
   pl_thumb_handle_t *pl_thumb   = (pl_thumb_handle_t*)calloc(1, sizeof(pl_thumb_handle_t));
   const char *playlist_file     = path_basename(playlist_path);
   
   /* Sanity check */
   if (!task || !pl_thumb ||
       string_is_empty(system) ||
       string_is_empty(playlist_path) ||
       string_is_empty(playlist_file))
      goto error;
   
   /* Only parse supported playlist types */
   if (string_is_equal(playlist_file, file_path_str(FILE_PATH_CONTENT_HISTORY)) ||
       string_is_equal(playlist_file, file_path_str(FILE_PATH_CONTENT_FAVORITES)) ||
       string_is_equal(playlist_file, file_path_str(FILE_PATH_CONTENT_MUSIC_HISTORY)) ||
       string_is_equal(playlist_file, file_path_str(FILE_PATH_CONTENT_VIDEO_HISTORY)) ||
       string_is_equal(playlist_file, file_path_str(FILE_PATH_CONTENT_IMAGE_HISTORY)) ||
       string_is_equal(system, "history") ||
       string_is_equal(system, "favorites") ||
       string_is_equal(system, "images_history"))
      goto error;
   
   /* Configure task */
   task->handler                 = task_pl_thumbnail_download_handler;
   task->state                   = pl_thumb;
   task->title                   = strdup(system);
   task->alternative_look        = true;
   task->progress                = 0;
   
   /* Configure handle */
   pl_thumb->system              = strdup(system);
   pl_thumb->playlist_path       = strdup(playlist_path);
   pl_thumb->playlist            = NULL;
   pl_thumb->thumbnail_path_data = NULL;
   pl_thumb->http_task           = NULL;
   pl_thumb->list_size           = 0;
   pl_thumb->list_index          = 0;
   pl_thumb->type_idx            = 1;
   pl_thumb->status              = PL_THUMB_BEGIN;
   
   task_queue_push(task);
   
   return true;
   
error:
   
   if (task)
   {
      free(task);
      task = NULL;
   }
   
   if (pl_thumb)
   {
      free(pl_thumb);
      pl_thumb = NULL;
   }
   
   return false;
}

/*************************************/
/* Playlist Entry Thumbnail Download */
/*************************************/

static void task_pl_entry_thumbnail_download_handler(retro_task_t *task)
{
   pl_thumb_handle_t *pl_thumb = NULL;
   
   if (!task)
   {
      free_pl_thumb_handle(pl_thumb, false);
      return;
   }
   
   pl_thumb = (pl_thumb_handle_t*)task->state;
   
   if (!pl_thumb)
      goto task_finished;
   
   if (task_get_cancelled(task))
      goto task_finished;
   
   switch (pl_thumb->status)
   {
      case PL_THUMB_BEGIN:
         {
            const char *label = NULL;
            
            /* Initialise thumbnail path data */
            pl_thumb->thumbnail_path_data = menu_thumbnail_path_init();
            
            if (!pl_thumb->thumbnail_path_data)
               goto task_finished;
            
            if (!menu_thumbnail_set_system(pl_thumb->thumbnail_path_data, pl_thumb->system))
               goto task_finished;
            
            if (!menu_thumbnail_set_content_playlist(
                  pl_thumb->thumbnail_path_data, pl_thumb->playlist, pl_thumb->list_index))
               goto task_finished;
            
            /* Set task title */
            task_free_title(task);
            if (menu_thumbnail_get_label(pl_thumb->thumbnail_path_data, &label))
               task_set_title(task, strdup(label));
            else
               task_set_title(task, strdup(""));
            task_set_progress(task, 0);
            
            /* All good - can start iterating */
            pl_thumb->status = PL_THUMB_ITERATE_TYPE;
         }
         break;
      case PL_THUMB_ITERATE_TYPE:
         {
            /* Ensure that we only enqueue one transfer
             * at a time... */
            if (pl_thumb->http_task)
            {
               if (task_get_finished(pl_thumb->http_task))
                  pl_thumb->http_task = NULL;
               else
                  break;
            }
            
            /* Check whether all thumbnail types have been processed */
            if (pl_thumb->type_idx > 3)
            {
               pl_thumb->status = PL_THUMB_END;
               break;
            }
            
            /* Update progress */
            task_set_progress(task, ((pl_thumb->type_idx - 1) * 100) / 3);
            
            /* Download current thumbnail */
            if (pl_thumb)
               download_pl_thumbnail(pl_thumb, true);
            
            /* Increment thumbnail type */
            pl_thumb->type_idx++;
         }
         break;
      case PL_THUMB_END:
      default:
         task_set_progress(task, 100);
         goto task_finished;
         break;
   }
   
   return;
   
task_finished:
   
   if (task)
      task_set_finished(task, true);
   
   free_pl_thumb_handle(pl_thumb, false);
}

bool task_push_pl_entry_thumbnail_download(
      const char *system, 
      const char *dir_thumbnails,
      playlist_t *playlist, unsigned idx)
{
   retro_task_t *task            = task_init();
   pl_thumb_handle_t *pl_thumb   = (pl_thumb_handle_t*)calloc(1, sizeof(pl_thumb_handle_t));
   
   /* Sanity check */
   if (!task || !pl_thumb || !playlist || string_is_empty(system))
      goto error;
   
   if (idx >= playlist_size(playlist))
      goto error;
   
   /* Only parse supported playlist types */
   if (string_is_equal(system, "images_history") ||
       string_is_equal(system, "music_history") ||
       string_is_equal(system, "video_history"))
      goto error;
   
   /* Configure task */
   task->handler                 = task_pl_entry_thumbnail_download_handler;
   task->state                   = pl_thumb;
   task->title                   = strdup(system);
   task->alternative_look        = true;
   task->progress                = 0;
   task->callback                = cb_task_pl_thumbnail_refresh_menu;
   
   /* Configure handle */
   pl_thumb->system              = strdup(system);
   pl_thumb->playlist_path       = NULL;
   pl_thumb->playlist            = playlist;
   pl_thumb->thumbnail_path_data = NULL;
   pl_thumb->http_task           = NULL;
   pl_thumb->list_size           = playlist_size(playlist);
   pl_thumb->list_index          = idx;
   pl_thumb->type_idx            = 1;
   pl_thumb->status              = PL_THUMB_BEGIN;
   pl_thumb->dir_thumbnails      = strdup(dir_thumbnails);
   
   task_queue_push(task);
   
   return true;
   
error:
   
   if (task)
   {
      free(task);
      task = NULL;
   }
   
   if (pl_thumb)
   {
      free(pl_thumb);
      pl_thumb = NULL;
   }
   
   return false;
}
