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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <string/stdstring.h>
#include <file/file_path.h>
#include <net/net_http.h>
#include <streams/file_stream.h>

#include "tasks_internal.h"
#include "task_file_transfer.h"

#include "../configuration.h"
#include "../file_path_special.h"
#include "../playlist.h"
#include "../verbosity.h"

#ifdef RARCH_INTERNAL
#include "../gfx/gfx_thumbnail_path.h"
#ifdef HAVE_MENU
#include "../menu/menu_cbs.h"
#include "../menu/menu_driver.h"
#else
#include <queues/message_queue.h>
#endif
#endif

enum pl_thumb_status
{
   PL_THUMB_BEGIN = 0,
   PL_THUMB_ITERATE_ENTRY,
   PL_THUMB_ITERATE_TYPE,
   PL_THUMB_END
};

enum pl_thumb_flags
{
   PL_THUMB_FLAG_OVERWRITE          = (1 << 0),
   PL_THUMB_FLAG_RIGHT_THUMB_EXISTS = (1 << 1),
   PL_THUMB_FLAG_LEFT_THUMB_EXISTS  = (1 << 2),
   PL_THUMB_FLAG_HTTP_TASK_COMPLETE = (1 << 3)
};

typedef struct pl_thumb_handle
{
   char *system;
   char *playlist_path;
   char *dir_thumbnails;
   playlist_t *playlist;
   gfx_thumbnail_path_data_t *thumbnail_path_data;
   retro_task_t *http_task;

   playlist_config_t playlist_config; /* size_t alignment */

   size_t list_size;
   size_t list_index;
   unsigned type_idx;

   enum pl_thumb_status status;
   enum playlist_thumbnail_name_flags name_flags;

   uint8_t flags;
} pl_thumb_handle_t;

typedef struct pl_entry_id
{
   char *playlist_path;
   size_t idx;
} pl_entry_id_t;

/*********************/
/* Utility Functions */
/*********************/

/* TODO/FIXME - Disable fetching of Named_Logos so far */

/* Fetches the thumbnail subdirectory (Named_Snaps,
 * Named_Titles, Named_Boxarts, Named_Logos) corresponding to the
 * specified 'type index' (1, 2, 3, 4).
 * Returns true if 'type index' is valid */
static bool gfx_thumbnail_get_sub_directory(
      unsigned type_idx, const char **sub_directory)
{
   if (!sub_directory)
      return false;

   switch (type_idx)
   {
      case 1:
         *sub_directory = "Named_Snaps";
         return true;
      case 2:
         *sub_directory = "Named_Titles";
         return true;
      case 3:
         *sub_directory = "Named_Boxarts";
         return true;
#if 0
      case 4:
         *sub_directory = "Named_Logos";
         return true;
#endif
      case 0:
      default:
         break;
   }

   return false;
}

/* Fetches current database name.
 * Returns true if database name is valid. */
static void gfx_thumbnail_get_db_name(
      gfx_thumbnail_path_data_t *path_data, const char **db_name)
{
   if (!string_is_empty(path_data->content_db_name))
      *db_name = path_data->content_db_name;
}

/* Fetches current thumbnail image name according to name flag
 * (name is the same for all thumbnail types).
 * Returns true if image name is valid. */
static bool gfx_thumbnail_get_img_name(
      gfx_thumbnail_path_data_t *path_data, const char **img_name,
      enum playlist_thumbnail_name_flags name_flags)
{
   if (!path_data || !img_name || name_flags == PLAYLIST_THUMBNAIL_FLAG_NONE)
      return false;

   if (name_flags & PLAYLIST_THUMBNAIL_FLAG_SHORT_NAME)
   {
      if (string_is_empty(path_data->content_img_short))
         return false;
      *img_name = path_data->content_img_short;
   }
   else if (name_flags & PLAYLIST_THUMBNAIL_FLAG_STD_NAME)
   {
      if (string_is_empty(path_data->content_img))
         return false;
      *img_name = path_data->content_img;
   }
   else if (name_flags & PLAYLIST_THUMBNAIL_FLAG_FULL_NAME)
   {
      if (string_is_empty(path_data->content_img_full))
         return false;
      *img_name = path_data->content_img_full;
   }
   else
      return false;

   return true;
}


/* Fetches local and remote paths for current thumbnail
 * of current type */
static bool task_pl_thumbnail_get_thumbnail_paths(
   pl_thumb_handle_t *pl_thumb,
   char *path, size_t path_size,
   char *s, size_t len)
{
   char content_dir[DIR_MAX_LENGTH];
   char tmp_buf[PATH_MAX_LENGTH];
   size_t raw_url_len      = sizeof(char) * 8192; /* TODO/FIXME - check size */
   char *raw_url           = NULL;
   const char *db_name     = NULL;
   const char *img_name    = NULL;
   const char *sub_dir     = NULL;
   const char *system_name = NULL;
   const char *system      = NULL;

   content_dir[0]          = '\0';

   if (!pl_thumb->thumbnail_path_data)
      return false;

   system = pl_thumb->thumbnail_path_data->system;

   if (string_is_empty(pl_thumb->dir_thumbnails))
      return false;

   /* Extract required strings */
   gfx_thumbnail_get_db_name(pl_thumb->thumbnail_path_data, &db_name);
   if (!gfx_thumbnail_get_img_name(pl_thumb->thumbnail_path_data, &img_name, pl_thumb->name_flags))
      return false;
   if (!gfx_thumbnail_get_sub_directory(pl_thumb->type_idx, &sub_dir))
      return false;

   /* Determine system name */
   if (string_is_empty(db_name))
   {
      if (string_is_empty(system))
         return false;

      /* If this is a content history or favorites playlist
       * then the current 'path_data->system' string is
       * meaningless. In this case, we fall back to the
       * content directory name */
      if (   string_is_equal(system, "history")
          || string_is_equal(system, "favorites"))
      {
         if (gfx_thumbnail_get_content_dir(
               pl_thumb->thumbnail_path_data, content_dir, sizeof(content_dir)) == 0)
            return false;

         system_name = content_dir;
      }
      else
         system_name = system;
   }
   else
      system_name = db_name;

   /* Generate local path */
   fill_pathname_join_special(path, pl_thumb->dir_thumbnails,
         system_name, path_size);
   fill_pathname_join_special(tmp_buf, path, sub_dir, sizeof(tmp_buf));
   fill_pathname_join_special(path, tmp_buf, img_name, path_size);

   if (string_is_empty(path))
      return false;
   if (!(raw_url = (char*)malloc(8192 * sizeof(char)))) /* TODO/FIXME - check size */
      return false;
   raw_url[0]     = '\0';

   /* Generate remote path */
   snprintf(raw_url, raw_url_len, "%s/%s/%s/%s",
         FILE_PATH_CORE_THUMBNAILS_URL,
         system_name, sub_dir, img_name);

   if (string_is_empty(raw_url))
   {
      free(raw_url);
      return false;
   }

   net_http_urlencode_full(s, raw_url, len);
   free(raw_url);

   return !string_is_empty(s);
}

/* Thumbnail download http task callback function
 * > Writes thumbnail file to disk */
void cb_http_task_download_pl_thumbnail(
      retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   char output_dir[DIR_MAX_LENGTH];
   http_transfer_data_t *data  = (http_transfer_data_t*)task_data;
   file_transfer_t *transf     = (file_transfer_t*)user_data;
   pl_thumb_handle_t *pl_thumb = NULL;

   /* Update pl_thumb task status
    * > Do this first, to minimise the risk of hanging
    *   the parent task in the event of an http error */
   if (!transf)
      goto finish;

   if (!(pl_thumb = (pl_thumb_handle_t*)transf->user_data))
      goto finish;

   pl_thumb->flags |= PL_THUMB_FLAG_HTTP_TASK_COMPLETE;

   /* Remaining sanity checks... */
   if (!data || !data->data || string_is_empty(transf->path))
      goto finish;

   /* Skip if data can't be good */
   if (data->status != 200)
   {
      err = "File not found.";
      goto finish;
   }

   /* Create output directory, if required */
   strlcpy(output_dir, transf->path, sizeof(output_dir));
   path_basedir_wrapper(output_dir);

   if (!path_mkdir(output_dir))
   {
      err = msg_hash_to_str(MSG_FAILED_TO_CREATE_THE_DIRECTORY);
      goto finish;
   }

   /* Write thumbnail file to disk */
   if (!filestream_write_file(transf->path, data->data, data->len))
   {
      err = "Write failed.";
      goto finish;
   }

finish:

   if (!string_is_empty(err))
      RARCH_ERR("[Thumbnail]: Download \"%s\" failed: %s\n",
            (transf ? transf->path : "unknown"), err);
   else
      RARCH_LOG("[Thumbnail]: Download \"%s\".\n",
            (transf ? transf->path : "unknown"));

   if (transf)
      free(transf);
}

/* Download thumbnail of the current type for the current
 * playlist entry */
static void download_pl_thumbnail(pl_thumb_handle_t *pl_thumb)
{
   char path[PATH_MAX_LENGTH];
   char url[2048];

   path[0] = '\0';
   url[0]  = '\0';

   /* Check if paths are valid */
   if (task_pl_thumbnail_get_thumbnail_paths(pl_thumb,
            path, sizeof(path),
            url,  sizeof(url)))
   {
      /* Only download missing thumbnails */
      if (     !path_is_valid(path)
            || (pl_thumb->flags & PL_THUMB_FLAG_OVERWRITE))
      {
         file_transfer_t *transf = (file_transfer_t*)malloc(sizeof(file_transfer_t));
         if (!transf)
            return; /* If this happens then everything is broken anyway... */

         /* Initialise http task status */
         pl_thumb->flags             &= ~PL_THUMB_FLAG_HTTP_TASK_COMPLETE;

         transf->enum_idx             = MSG_UNKNOWN;
         transf->path[0]              = '\0';
         /* Initialise file transfer */
         transf->user_data            = (void*)pl_thumb;
         strlcpy(transf->path, path, sizeof(transf->path));

         /* Note: We don't actually care if this fails since that
          * just means the file is missing from the server, so it's
          * not something we can handle here... */

         /* ...if it does fail, however, we can immediately
          * signal that the task is 'complete' */
         if (!(pl_thumb->http_task = (retro_task_t*)task_push_http_transfer_file(
               url, true, NULL, cb_http_task_download_pl_thumbnail, transf)))
            pl_thumb->flags             |= PL_THUMB_FLAG_HTTP_TASK_COMPLETE;
      }
   }
}

static void free_pl_thumb_handle(pl_thumb_handle_t *pl_thumb)
{
   if (pl_thumb->system)
   {
      free(pl_thumb->system);
      pl_thumb->system = NULL;
   }

   if (pl_thumb->playlist_path)
   {
      free(pl_thumb->playlist_path);
      pl_thumb->playlist_path = NULL;
   }

   if (pl_thumb->dir_thumbnails)
   {
      free(pl_thumb->dir_thumbnails);
      pl_thumb->dir_thumbnails = NULL;
   }

   if (pl_thumb->playlist)
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

/*******************************/
/* Playlist Thumbnail Download */
/*******************************/

static void task_pl_thumbnail_download_handler(retro_task_t *task)
{
   uint8_t flg;
   pl_thumb_handle_t *pl_thumb = NULL;
   enum playlist_thumbnail_name_flags next_flag = PLAYLIST_THUMBNAIL_FLAG_INVALID;

   if (!task)
      goto task_finished;

   if (!(pl_thumb = (pl_thumb_handle_t*)task->state))
      goto task_finished;

   flg = task_get_flags(task);

   if ((flg & RETRO_TASK_FLG_CANCELLED) > 0)
      goto task_finished;

   switch (pl_thumb->status)
   {
      case PL_THUMB_BEGIN:
         /* Load playlist */
         if (!path_is_valid(pl_thumb->playlist_config.path))
            goto task_finished;

         if (!(pl_thumb->playlist = playlist_init(&pl_thumb->playlist_config)))
            goto task_finished;

         pl_thumb->list_size = playlist_size(pl_thumb->playlist);

         if (pl_thumb->list_size < 1)
            goto task_finished;

         /* Initialise thumbnail path data */
         if (!(pl_thumb->thumbnail_path_data = gfx_thumbnail_path_init()))
            goto task_finished;

         if (!gfx_thumbnail_set_system(
                  pl_thumb->thumbnail_path_data,
                  pl_thumb->system, pl_thumb->playlist))
            goto task_finished;

         /* All good - can start iterating */
         pl_thumb->status = PL_THUMB_ITERATE_ENTRY;
         break;
      case PL_THUMB_ITERATE_ENTRY:
         /* Set current thumbnail content */
         if (gfx_thumbnail_set_content_playlist(
                  pl_thumb->thumbnail_path_data, pl_thumb->playlist, pl_thumb->list_index))
         {
            /* Update progress display */
            task_free_title(task);
            if (!string_is_empty(pl_thumb->thumbnail_path_data->content_label))
               task_set_title(task, strdup(pl_thumb->thumbnail_path_data->content_label));
            else
               task_set_title(task, strdup(""));
            task_set_progress(task, (pl_thumb->list_index * 100) / pl_thumb->list_size);

            /* Start iterating over thumbnail type */
            pl_thumb->type_idx  = 1;
            pl_thumb->status    = PL_THUMB_ITERATE_TYPE;
            playlist_update_thumbnail_name_flag(pl_thumb->playlist, pl_thumb->list_index, PLAYLIST_THUMBNAIL_FLAG_FULL_NAME);
            pl_thumb->name_flags = PLAYLIST_THUMBNAIL_FLAG_FULL_NAME;
         }
         else
         {
            /* Current playlist entry is broken - advance to
             * the next one */
            pl_thumb->list_index++;
            if (pl_thumb->list_index >= pl_thumb->list_size)
               pl_thumb->status = PL_THUMB_END;
         }
         break;
      case PL_THUMB_ITERATE_TYPE:
         /* Ensure that we only enqueue one transfer
          * at a time... */

         /* > If HTTP task is NULL, then it either finished
          *   or an error occurred - in either case,
          *   current task is 'complete' */
         if (!pl_thumb->http_task)
            pl_thumb->flags             |= PL_THUMB_FLAG_HTTP_TASK_COMPLETE;
         /* > Wait for task_push_http_transfer_file()
          *   callback to trigger */
         else if (!(pl_thumb->flags & PL_THUMB_FLAG_HTTP_TASK_COMPLETE))
            break;

         pl_thumb->http_task = NULL;

         /* Check whether all thumbnail types have been processed */
         /* TODO/FIXME - turn 3 into 4 when we re-enable Named_Logos for fetching */
         if (pl_thumb->type_idx > 3)
         {
            next_flag = playlist_get_next_thumbnail_name_flag(pl_thumb->playlist,pl_thumb->list_index);
            if (next_flag == PLAYLIST_THUMBNAIL_FLAG_NONE)
            {
               if (pl_thumb->playlist )
               /* Time to move on to the next entry */
               pl_thumb->list_index++;
               if (pl_thumb->list_index < pl_thumb->list_size)
                  pl_thumb->status = PL_THUMB_ITERATE_ENTRY;
               else
                  pl_thumb->status = PL_THUMB_END;
               break;
            }
            else
            {
               /* Increment the name flag to cover the 3 supported naming conventions.
                * Side-effect: all combinations will be tried (3x3 requests for 1 playlist entry)
                * even if some files were already downloaded, but that may be useful if later on
                * different view priorities are implemented. */
               pl_thumb->type_idx = 1;
               playlist_update_thumbnail_name_flag(pl_thumb->playlist, pl_thumb->list_index, next_flag);
               pl_thumb->name_flags = next_flag;
            }
         }

         /* Download current thumbnail */
         if (pl_thumb)
            download_pl_thumbnail(pl_thumb);

         /* Increment thumbnail type */
         pl_thumb->type_idx++;
         break;
      case PL_THUMB_END:
      default:
         task_set_progress(task, 100);
         goto task_finished;
   }

   return;

task_finished:
   if (task)
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
   if (pl_thumb)
      free_pl_thumb_handle(pl_thumb);
}

static bool task_pl_thumbnail_finder(retro_task_t *task, void *user_data)
{
   if (task && user_data && task->handler == task_pl_thumbnail_download_handler)
   {
      pl_thumb_handle_t *pl_thumb = NULL;
      if ((pl_thumb = (pl_thumb_handle_t*)task->state))
         return string_is_equal((const char*)user_data, pl_thumb->playlist_config.path);
   }
   return false;
}

bool task_push_pl_thumbnail_download(
      const char *system,
      const playlist_config_t *playlist_config,
      const char *dir_thumbnails)
{
   task_finder_data_t find_data;
   const char *playlist_file     = NULL;
   retro_task_t *task            = task_init();
   pl_thumb_handle_t *pl_thumb   = (pl_thumb_handle_t*)calloc(1, sizeof(pl_thumb_handle_t));

   /* Sanity check */
   if (!playlist_config || !task || !pl_thumb)
      goto error;

   if (   string_is_empty(system)
       || string_is_empty(playlist_config->path)
       || string_is_empty(dir_thumbnails))
      goto error;

   playlist_file                 = path_basename_nocompression(
         playlist_config->path);

   if (string_is_empty(playlist_file))
      goto error;

   /* Only parse supported playlist types */
   if (
            string_ends_with_size(playlist_file, "_history.lpl",
               strlen(playlist_file),
               STRLEN_CONST("_history.lpl")
               )
         || string_is_equal(playlist_file,
            FILE_PATH_CONTENT_FAVORITES)
         || string_is_equal(system, "history")
         || string_is_equal(system, "favorites"))
      goto error;

   /* Concurrent download of thumbnails for the same
    * playlist is not allowed */
   find_data.func                = task_pl_thumbnail_finder;
   find_data.userdata            = (void*)playlist_config->path;

   if (task_queue_find(&find_data))
      goto error;

   /* Configure handle */
   if (!playlist_config_copy(playlist_config, &pl_thumb->playlist_config))
      goto error;

   pl_thumb->system              = strdup(system);
   pl_thumb->playlist_path       = NULL;
   pl_thumb->dir_thumbnails      = strdup(dir_thumbnails);
   pl_thumb->playlist            = NULL;
   pl_thumb->thumbnail_path_data = NULL;
   pl_thumb->http_task           = NULL;
   pl_thumb->list_size           = 0;
   pl_thumb->list_index          = 0;
   pl_thumb->type_idx            = 1;
   pl_thumb->status              = PL_THUMB_BEGIN;

   /* Configure task */
   task->handler                 = task_pl_thumbnail_download_handler;
   task->state                   = pl_thumb;
   task->title                   = strdup(system);
   task->progress                = 0;
   task->flags                  |= RETRO_TASK_FLG_ALTERNATIVE_LOOK;

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

static void cb_task_pl_entry_thumbnail_refresh_menu(
      retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
#if defined(RARCH_INTERNAL) && defined(HAVE_MENU)
   pl_thumb_handle_t *pl_thumb     = NULL;
   bool do_refresh                 = false;
   playlist_t *current_playlist    = playlist_get_cached();
   menu_handle_t *menu             = menu_state_get_ptr()->driver_data;
#ifdef HAVE_MATERIALUI
   const char *menu_driver         = menu_driver_ident();
#endif

   if (!task)
      return;

   pl_thumb                        = (pl_thumb_handle_t*)task->state;

   if (!pl_thumb || !pl_thumb->thumbnail_path_data)
      return;

   /* Only refresh if current playlist hasn't changed,
    * and menu selection pointer is on the same entry
    * (Note: this is crude, but it's sufficient to prevent
    * 'refresh' from getting spammed when switching
    * playlists or scrolling through one playlist at
    * maximum speed with on demand downloads enabled)
    * NOTE: GLUI requires special treatment, since
    * it displays multiple thumbnails at a time... */
   if (!current_playlist || !menu)
      return;
   if (string_is_empty(playlist_get_conf_path(current_playlist)))
      return;

#ifdef HAVE_MATERIALUI
   if (string_is_equal(menu_driver, "glui"))
   {
      if (!string_is_equal(pl_thumb->playlist_path,
            playlist_get_conf_path(current_playlist)))
         return;
   }
   else
#endif
   {
      if (!string_is_equal(pl_thumb->playlist_path,
            playlist_get_conf_path(current_playlist)))
         return;
   }

   /* Only refresh if left/right thumbnails did not exist
    * when the task began, but do exist now
    * (with the caveat that we must also refresh if existing
    * files have been overwritten) */

   if (   !(pl_thumb->flags & PL_THUMB_FLAG_RIGHT_THUMB_EXISTS)
         ||(pl_thumb->flags & PL_THUMB_FLAG_OVERWRITE))
      if (gfx_thumbnail_update_path(pl_thumb->thumbnail_path_data, GFX_THUMBNAIL_RIGHT))
      {
         if (!string_is_empty(pl_thumb->thumbnail_path_data->right_path))
            do_refresh = path_is_valid(pl_thumb->thumbnail_path_data->right_path);
      }

   if (!do_refresh)
      if (    !(pl_thumb->flags & PL_THUMB_FLAG_LEFT_THUMB_EXISTS)
            || (pl_thumb->flags & PL_THUMB_FLAG_OVERWRITE))
      {
         if (gfx_thumbnail_update_path(pl_thumb->thumbnail_path_data, GFX_THUMBNAIL_LEFT))
         {
            if (!string_is_empty(pl_thumb->thumbnail_path_data->left_path))
               do_refresh = path_is_valid(pl_thumb->thumbnail_path_data->left_path);
         }
      }

   if (do_refresh)
   {
      struct menu_state *menu_st = menu_state_get_ptr();
      unsigned i                 = (unsigned)pl_thumb->list_index;
      if (     menu_st->driver_ctx
            && menu_st->driver_ctx->refresh_thumbnail_image)
         menu_st->driver_ctx->refresh_thumbnail_image(
               menu_st->userdata, i);

   }

#endif
}

static void task_pl_entry_thumbnail_free(retro_task_t *task)
{
   pl_thumb_handle_t *pl_thumb = NULL;
   if (task && (pl_thumb = (pl_thumb_handle_t*)task->state))
      free_pl_thumb_handle(pl_thumb);
}

static void task_pl_entry_thumbnail_download_handler(retro_task_t *task)
{
   uint8_t flg;
   pl_thumb_handle_t *pl_thumb = NULL;

   if (!task)
      return;

   if (!(pl_thumb = (pl_thumb_handle_t*)task->state))
      goto task_finished;

   flg = task_get_flags(task);

   if ((flg & RETRO_TASK_FLG_CANCELLED) > 0)
      goto task_finished;

   switch (pl_thumb->status)
   {
      case PL_THUMB_BEGIN:
         /* Check whether current right/left thumbnails
          * already exist (required for menu refresh callback) */
         pl_thumb->flags &= ~PL_THUMB_FLAG_RIGHT_THUMB_EXISTS;
         pl_thumb->flags &= ~PL_THUMB_FLAG_LEFT_THUMB_EXISTS;

         if (gfx_thumbnail_update_path(pl_thumb->thumbnail_path_data,
                  GFX_THUMBNAIL_RIGHT))
         {
            if (     !string_is_empty(pl_thumb->thumbnail_path_data->right_path)
                  && path_is_valid(pl_thumb->thumbnail_path_data->right_path))
               pl_thumb->flags |= PL_THUMB_FLAG_RIGHT_THUMB_EXISTS;
         }

         if (gfx_thumbnail_update_path(pl_thumb->thumbnail_path_data,
                  GFX_THUMBNAIL_LEFT))
         {
            if (     !string_is_empty(pl_thumb->thumbnail_path_data->left_path)
                  && path_is_valid(pl_thumb->thumbnail_path_data->left_path))
               pl_thumb->flags |= PL_THUMB_FLAG_LEFT_THUMB_EXISTS;
         }

         /* Set task title */
         task_free_title(task);
         if (!string_is_empty(
                  pl_thumb->thumbnail_path_data->content_label))
            task_set_title(task, strdup(pl_thumb->thumbnail_path_data->content_label));
         else
            task_set_title(task, strdup(""));
         task_set_progress(task, 0);

         /* All good - can start iterating */
         pl_thumb->status = PL_THUMB_ITERATE_TYPE;
         break;
      case PL_THUMB_ITERATE_TYPE:
         {
            /* Ensure that we only enqueue one transfer
             * at a time... */

            /* > If HTTP task is NULL, then it either finished
             *   or an error occurred - in either case,
             *   current task is 'complete' */
            if (!pl_thumb->http_task)
               pl_thumb->flags |= PL_THUMB_FLAG_HTTP_TASK_COMPLETE;
            /* > Wait for task_push_http_transfer_file()
             *   callback to trigger */
            else if (!(pl_thumb->flags & PL_THUMB_FLAG_HTTP_TASK_COMPLETE))
               break;

            pl_thumb->http_task = NULL;

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
               download_pl_thumbnail(pl_thumb);

            /* Increment thumbnail type */
            pl_thumb->type_idx++;
         }
         break;
      case PL_THUMB_END:
      default:
         task_set_progress(task, 100);
         goto task_finished;
   }

   return;

task_finished:
   if (task)
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static bool task_pl_entry_thumbnail_finder(retro_task_t *task, void *user_data)
{
   pl_entry_id_t *entry_id     = NULL;
   if (     task
         && user_data
         && task->handler == task_pl_entry_thumbnail_download_handler
         && (entry_id = (pl_entry_id_t*)user_data))
   {
      pl_thumb_handle_t *pl_thumb = NULL;
      if ((pl_thumb = (pl_thumb_handle_t*)task->state))
         return (entry_id->idx == pl_thumb->list_index) &&
            string_is_equal(entry_id->playlist_path, pl_thumb->playlist_path);
   }
   return false;
}

bool task_push_pl_entry_thumbnail_download(
      const char *system,
      playlist_t *playlist,
      unsigned idx,
      bool overwrite,
      bool mute)
{
   task_finder_data_t find_data;
   settings_t *settings          = config_get_ptr();
   retro_task_t *task            = task_init();
   pl_thumb_handle_t *pl_thumb   = (pl_thumb_handle_t*)calloc(1, sizeof(pl_thumb_handle_t));
   pl_entry_id_t *entry_id       = (pl_entry_id_t*)malloc(sizeof(pl_entry_id_t));
   char *playlist_path           = NULL;
   gfx_thumbnail_path_data_t *
         thumbnail_path_data     = NULL;
   const char *dir_thumbnails    = NULL;
   enum playlist_thumbnail_name_flags next_flag = PLAYLIST_THUMBNAIL_FLAG_INVALID;

   /* Sanity check */
   if (!settings || !task || !pl_thumb || !playlist || !entry_id)
      goto error;

   dir_thumbnails                = settings->paths.directory_thumbnails;

   if (   string_is_empty(system)
       || string_is_empty(dir_thumbnails)
       || string_is_empty(playlist_get_conf_path(playlist)))
      goto error;

   if (idx >= playlist_size(playlist))
      goto error;

   /* Only parse supported playlist types */
   if (string_ends_with_size(system, "_history",
            strlen(system),
            STRLEN_CONST("_history")
            ))
      goto error;

   /* Copy playlist path
    * (required for task finder and menu refresh functionality) */
   playlist_path                 = strdup(playlist_get_conf_path(playlist));

   /* Concurrent download of thumbnails for the same
    * playlist entry is not allowed */
   entry_id->playlist_path       = playlist_path;
   entry_id->idx                 = idx;

   find_data.func                = task_pl_entry_thumbnail_finder;
   find_data.userdata            = (void*)entry_id;

   if (task_queue_find(&find_data))
      goto error;

   free(entry_id);
   entry_id = NULL;

   /* Initialise thumbnail path data
    * > Have to do this here rather than in the
    *   task handler to avoid thread race conditions */
   thumbnail_path_data = gfx_thumbnail_path_init();

   if (!thumbnail_path_data)
      goto error;

   if (!gfx_thumbnail_set_system(
         thumbnail_path_data, system, playlist))
      goto error;

   if (!gfx_thumbnail_set_content_playlist(
         thumbnail_path_data, playlist, idx))
      goto error;

   /* Apply flexible thumbnail naming: ROM file name - database name - short name */
   next_flag = playlist_get_next_thumbnail_name_flag(playlist,idx);
   playlist_update_thumbnail_name_flag(playlist, idx, next_flag);
   /* Skip over full name if not enabled. */
   if (next_flag == PLAYLIST_THUMBNAIL_FLAG_FULL_NAME && !settings->bools.playlist_use_filename)
   {
      next_flag = playlist_get_next_thumbnail_name_flag(playlist,idx);
      playlist_update_thumbnail_name_flag(playlist, idx, next_flag);
   }
   if (next_flag == PLAYLIST_THUMBNAIL_FLAG_NONE)
   {
      const char *_msg = msg_hash_to_str(MSG_NO_THUMBNAIL_DOWNLOAD_POSSIBLE);
      runloop_msg_queue_push(_msg, strlen(_msg), 1, 100, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      goto error;

   }

   /* Configure handle
    * > Note: playlist_config is unused by this task */
   pl_thumb->system              = NULL;
   pl_thumb->playlist_path       = playlist_path;
   pl_thumb->dir_thumbnails      = strdup(dir_thumbnails);
   pl_thumb->playlist            = NULL;
   pl_thumb->thumbnail_path_data = thumbnail_path_data;
   pl_thumb->http_task           = NULL;
   pl_thumb->list_size           = playlist_size(playlist);
   pl_thumb->list_index          = idx;
   pl_thumb->type_idx            = 1;
   pl_thumb->name_flags          = next_flag;
   pl_thumb->status              = PL_THUMB_BEGIN;

   if (overwrite)
      pl_thumb->flags            = PL_THUMB_FLAG_OVERWRITE;

   /* Configure task */
   task->handler                 = task_pl_entry_thumbnail_download_handler;
   task->state                   = pl_thumb;
   task->title                   = strdup(system);
   task->progress                = 0;
   task->callback                = cb_task_pl_entry_thumbnail_refresh_menu;
   task->cleanup                 = task_pl_entry_thumbnail_free;
   task->flags                  |=  RETRO_TASK_FLG_ALTERNATIVE_LOOK;
   if (mute)
      task->flags               |=  RETRO_TASK_FLG_MUTE;
   else
      task->flags               &= ~RETRO_TASK_FLG_MUTE;

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

   if (entry_id)
   {
      free(entry_id);
      entry_id = NULL;
   }

   if (playlist_path)
   {
      free(playlist_path);
      playlist_path = NULL;
   }

   if (thumbnail_path_data)
   {
      free(thumbnail_path_data);
      thumbnail_path_data = NULL;
   }

   return false;
}
