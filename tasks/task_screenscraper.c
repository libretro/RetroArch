/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - RetroArch
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation, either version 3 of the License, or (at your option)
 *  any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with RetroArch. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <retro_miscellaneous.h>
#include <string/stdstring.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <net/net_http.h>

#include "task_file_transfer.h"
#include "tasks_internal.h"

#include "../configuration.h"
#include "../file_path_special.h"
#include "../gfx/gfx_thumbnail.h"
#include "../msg_hash.h"
#include "../network/screenscraper.h"
#include "../playlist.h"
#include "../verbosity.h"

enum ss_scrape_status
{
   SS_SCRAPE_BEGIN = 0,
   SS_SCRAPE_ITERATE_ENTRY,
   SS_SCRAPE_WAIT_INFO,
   SS_SCRAPE_ITERATE_MEDIA,
   SS_SCRAPE_WAIT_MEDIA,
   SS_SCRAPE_END
};

enum ss_scrape_flags
{
   SS_SCRAPE_FLAG_HTTP_COMPLETE = (1 << 0),
   SS_SCRAPE_FLAG_INFO_VALID    = (1 << 1)
};

typedef struct ss_scrape_handle
{
   char *system;
   char *playlist_path;
   char *dir_thumbnails;
   playlist_t *playlist;
   gfx_thumbnail_path_data_t *thumbnail_path_data;
   retro_task_t *http_task;
   screenscraper_game_t *game;

   /* Copies of the relevant settings, so mid-scrape changes
    * cannot corrupt the run */
   char username[64];
   char password[64];
   char devid[64];
   char devpassword[64];
   char region[8];
   char language[8];

   playlist_config_t playlist_config; /* size_t alignment */

   size_t list_size;
   size_t list_index;
   unsigned media_kind;               /* enum screenscraper_media_kind */
   /* Source attempt for the current media kind: indexes the ordered
    * source chain (ScreenScraper / libretro thumbnail server) */
   unsigned attempt;

   enum ss_scrape_status status;

   bool media_enabled[SS_MEDIA_LAST];
   bool metadata_enabled;
   bool overwrite;
   bool use_crc;
   /* Scraper order: when set, the libretro thumbnail server is tried
    * first and ScreenScraper acts as the fallback */
   bool prefer_libretro;
   /* On-demand mode: scrape a single playlist entry and stop */
   bool single_entry;
   /* What to do when the daily allowance runs out: pause and save a
    * resume point, or carry on with the libretro server alone */
   bool quota_continue_libretro;
   /* Set once the allowance ran out during this run */
   bool quota_hit;

   uint8_t flags;
} ss_scrape_handle_t;

/*********************/
/* Utility Functions */
/*********************/

/* Builds the local destination path for the given media kind of the
 * current playlist entry:
 * <dir_thumbnails>/<db_name>/<sub_dir>/<image name>.<ext> */
static bool ss_scrape_get_media_path(ss_scrape_handle_t *ss,
      const screenscraper_media_t *media,
      enum screenscraper_media_kind kind,
      char *path, size_t path_size)
{
   char tmp_buf[PATH_MAX_LENGTH];
   char file_name[NAME_MAX_LENGTH];
   const char *system_name = NULL;
   const char *img_name    = NULL;
   const char *sub_dir     = screenscraper_media_sub_dir(kind);
   const char *ext         = NULL;

   if (!ss->thumbnail_path_data || !sub_dir)
      return false;
   if (string_is_empty(ss->dir_thumbnails))
      return false;

   /* Playlist database name has priority over system name */
   if (*ss->thumbnail_path_data->content_db_name)
      system_name = ss->thumbnail_path_data->content_db_name;
   else if (*ss->thumbnail_path_data->system)
      system_name = ss->thumbnail_path_data->system;
   else
      return false;

   /* content_img is the label-derived name every menu driver looks for
    * first; content_img_full is only populated when the full file name
    * differs from it, so it cannot be used on its own. */
   if (*ss->thumbnail_path_data->content_img)
      img_name = ss->thumbnail_path_data->content_img;
   else if (*ss->thumbnail_path_data->content_img_full)
      img_name = ss->thumbnail_path_data->content_img_full;
   else
      return false;

   /* The classic thumbnail types (and logos) must keep the ".png"
    * name menu drivers expect; extended media adopt the extension the
    * service reports (mp4, pdf, ...) */
   strlcpy(file_name, img_name, sizeof(file_name));
   if (kind > SS_MEDIA_LOGO)
   {
      if (media && !string_is_empty(media->format))
         ext = media->format;
      else
         ext = screenscraper_media_default_ext(kind);

      if (!string_is_equal(ext, "png"))
      {
         path_remove_extension(file_name);
         strlcat(file_name, ".", sizeof(file_name));
         strlcat(file_name, ext, sizeof(file_name));
      }
   }

   fill_pathname_join_special(tmp_buf, ss->dir_thumbnails,
         system_name, sizeof(tmp_buf));
   fill_pathname_join_special(path, tmp_buf, sub_dir, path_size);
   fill_pathname_join_special(tmp_buf, path, file_name, sizeof(tmp_buf));
   strlcpy(path, tmp_buf, path_size);

   return !string_is_empty(path);
}

/* Builds the metadata sidecar path:
 * <dir_thumbnails>/<db_name>/Metadata/<image name>.json */
static bool ss_scrape_get_metadata_path(ss_scrape_handle_t *ss,
      char *path, size_t path_size)
{
   char tmp_buf[PATH_MAX_LENGTH];
   char file_name[NAME_MAX_LENGTH];
   const char *system_name = NULL;

   if (!ss->thumbnail_path_data)
      return false;
   if (string_is_empty(ss->dir_thumbnails))
      return false;

   if (*ss->thumbnail_path_data->content_db_name)
      system_name = ss->thumbnail_path_data->content_db_name;
   else if (*ss->thumbnail_path_data->system)
      system_name = ss->thumbnail_path_data->system;
   else
      return false;

   if (*ss->thumbnail_path_data->content_img)
      strlcpy(file_name, ss->thumbnail_path_data->content_img,
            sizeof(file_name));
   else if (*ss->thumbnail_path_data->content_img_full)
      strlcpy(file_name, ss->thumbnail_path_data->content_img_full,
            sizeof(file_name));
   else
      return false;

   path_remove_extension(file_name);
   strlcat(file_name, ".json", sizeof(file_name));

   fill_pathname_join_special(tmp_buf, ss->dir_thumbnails,
         system_name, sizeof(tmp_buf));
   fill_pathname_join_special(path, tmp_buf, "Metadata", path_size);
   fill_pathname_join_special(tmp_buf, path, file_name, sizeof(tmp_buf));
   strlcpy(path, tmp_buf, path_size);

   return !string_is_empty(path);
}

static void ss_scrape_write_metadata(ss_scrape_handle_t *ss)
{
   char path[PATH_MAX_LENGTH];
   char output_dir[PATH_MAX_LENGTH];
   char *json = NULL;

   if (!ss->metadata_enabled || !ss->game)
      return;
   if (!ss_scrape_get_metadata_path(ss, path, sizeof(path)))
      return;
   if (path_is_valid(path) && !ss->overwrite)
      return;
   if (!(json = screenscraper_metadata_to_json(ss->game)))
      return;

   strlcpy(output_dir, path, sizeof(output_dir));
   path_basedir_wrapper(output_dir);

   if (!path_mkdir(output_dir))
      RARCH_ERR("[ScreenScraper] Failed to create \"%s\".\n", output_dir);
   else if (!filestream_write_file(path, json, strlen(json)))
      RARCH_ERR("[ScreenScraper] Failed to write \"%s\".\n", path);
   else
      RARCH_LOG("[ScreenScraper] Metadata \"%s\".\n", path);

   free(json);
}

/* Resume point for a scrape interrupted by the request allowance.
 * Stored beside the thumbnails so it travels with the media tree. */
static void ss_scrape_resume_path(const char *dir_thumbnails,
      char *s, size_t len)
{
   fill_pathname_join_special(s, dir_thumbnails,
         "screenscraper_resume.txt", len);
}

static void ss_scrape_resume_save(ss_scrape_handle_t *ss)
{
   char path[PATH_MAX_LENGTH];
   char buf[PATH_MAX_LENGTH + 256];
   size_t _len;

   if (ss->single_entry || string_is_empty(ss->dir_thumbnails))
      return;

   ss_scrape_resume_path(ss->dir_thumbnails, path, sizeof(path));

   /* system, entry index and playlist path, one per line */
   _len  = strlcpy(buf, ss->system ? ss->system : "", sizeof(buf));
   _len += snprintf(buf + _len, sizeof(buf) - _len, "\n%u\n",
         (unsigned)ss->list_index);
   strlcpy(buf + _len, ss->playlist_config.path, sizeof(buf) - _len);

   if (filestream_write_file(path, buf, strlen(buf)))
      RARCH_LOG("[ScreenScraper] Paused at entry %u of \"%s\".\n",
            (unsigned)ss->list_index, ss->system ? ss->system : "");
}

static void ss_scrape_resume_clear(const char *dir_thumbnails)
{
   char path[PATH_MAX_LENGTH];

   if (string_is_empty(dir_thumbnails))
      return;

   ss_scrape_resume_path(dir_thumbnails, path, sizeof(path));
   if (path_is_valid(path))
      filestream_delete(path);
}

/* jeuInfos response callback: parse the game description */
static void ss_scrape_info_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   http_transfer_data_t *data = (http_transfer_data_t*)task_data;
   ss_scrape_handle_t *ss     = (ss_scrape_handle_t*)user_data;

   if (!ss)
      return;

   ss->flags |= SS_SCRAPE_FLAG_HTTP_COMPLETE;

   if (!data || !data->data)
   {
      RARCH_WARN("[ScreenScraper] Game lookup returned no data.\n");
      return;
   }

   if (screenscraper_status_is_quota(data->status, data->data, data->len))
   {
      screenscraper_set_quota_exhausted(true);
      ss->quota_hit = true;
      RARCH_WARN("[ScreenScraper] Daily request allowance reached.\n");
      return;
   }

   if (data->status != 200)
   {
      /* The service explains parameter/quota problems in the response
       * body; surfacing a snippet turns an opaque status code into an
       * actionable message. */
      char reason[160];
      size_t _len = (data->len < sizeof(reason) - 1)
            ? data->len : sizeof(reason) - 1;
      size_t i;

      memcpy(reason, data->data, _len);
      reason[_len] = '\0';
      for (i = 0; i < _len; i++)
         if (reason[i] == '\n' || reason[i] == '\r')
            reason[i] = ' ';

      RARCH_WARN("[ScreenScraper] Game lookup failed (HTTP %d): %s\n",
            data->status, reason);
      return;
   }

   {
      screenscraper_creds_t creds;
      creds.devid       = ss->devid;
      creds.devpassword = ss->devpassword;
      creds.username    = ss->username;
      creds.password    = ss->password;
      creds.region      = ss->region;
      creds.language    = ss->language;

      if (ss->game)
         screenscraper_game_free(ss->game);
      if ((ss->game = screenscraper_parse_game_info(
            data->data, data->len, &creds)))
         ss->flags |= SS_SCRAPE_FLAG_INFO_VALID;
   }
}

/* Media download callback: write the file to disk */
static void ss_scrape_media_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   char output_dir[DIR_MAX_LENGTH];
   http_transfer_data_t *data = (http_transfer_data_t*)task_data;
   file_transfer_t *transf    = (file_transfer_t*)user_data;
   ss_scrape_handle_t *ss     = NULL;

   if (!transf)
      goto finish;

   if (!(ss = (ss_scrape_handle_t*)transf->user_data))
      goto finish;

   ss->flags |= SS_SCRAPE_FLAG_HTTP_COMPLETE;

   if (!data || !data->data || !*transf->path)
      goto finish;

   if (data->status != 200)
   {
      err = "File not found.";
      goto finish;
   }

   strlcpy(output_dir, transf->path, sizeof(output_dir));
   path_basedir_wrapper(output_dir);

   if (!path_mkdir(output_dir))
   {
      err = msg_hash_to_str(MSG_FAILED_TO_CREATE_THE_DIRECTORY);
      goto finish;
   }

   if (!filestream_write_file(transf->path, data->data, data->len))
   {
      err = "Write failed.";
      goto finish;
   }

finish:
   if (err && *err)
      RARCH_ERR("[ScreenScraper] Download \"%s\" failed: %s\n",
            (transf ? transf->path : "unknown"), err);
   else
      RARCH_LOG("[ScreenScraper] Downloaded \"%s\".\n",
            (transf ? transf->path : "unknown"));

   if (transf)
      free(transf);
}

/* Requests jeuInfos for the current playlist entry.
 * Returns false when the entry cannot be queried at all
 * (missing credentials or unusable entry). */
static bool ss_scrape_request_info(ss_scrape_handle_t *ss)
{
   char url[2048];
   char rom_name[NAME_MAX_LENGTH];
   const struct playlist_entry *entry = NULL;
   const char *content_path           = NULL;
   char crc32[16];
   screenscraper_creds_t creds;
   unsigned system_id;
   int64_t rom_size                   = 0;

   crc32[0]    = '\0';
   rom_name[0] = '\0';

   playlist_get_index(ss->playlist, ss->list_index, &entry);
   if (!entry)
      return false;

   /* ROM name: base filename of the content (strip any archive
    * sub-path: "archive.zip#rom.bin" -> "archive.zip") */
   content_path = entry->path;
   if (!string_is_empty(content_path))
   {
      char tmp[PATH_MAX_LENGTH];
      const char *archive_delim = NULL;

      strlcpy(tmp, content_path, sizeof(tmp));
      if ((archive_delim = path_get_archive_delim(tmp)))
         tmp[archive_delim - tmp] = '\0';

      strlcpy(rom_name, path_basename(tmp), sizeof(rom_name));
      rom_size = path_get_size(tmp);
   }

   /* The playlist 'crc32' field is dual purpose: it holds either
    * "XXXXXXXX|crc" or "<SERIAL>|serial" (disc based systems store the
    * serial there). Only a genuine 8 digit hex CRC may be sent as the
    * 'crc' parameter - a serial makes the API reject the whole request
    * with HTTP 400, so in that case identification falls back to file
    * name plus size. */
   if (ss->use_crc && !string_is_empty(entry->crc32))
   {
      char tmp[32];
      char *delim = NULL;

      strlcpy(tmp, entry->crc32, sizeof(tmp));

      if ((delim = strchr(tmp, '|')))
      {
         *delim = '\0';
         if (string_is_equal(delim + 1, "crc"))
         {
            size_t i;
            size_t _len   = strlen(tmp);
            bool valid    = (_len == 8);
            bool all_zero = true;

            for (i = 0; valid && i < _len; i++)
            {
               if (!isxdigit((unsigned char)tmp[i]))
                  valid = false;
               else if (tmp[i] != '0')
                  all_zero = false;
            }

            if (valid && !all_zero)
               strlcpy(crc32, tmp, sizeof(crc32));
         }
      }
   }

   creds.devid       = ss->devid;
   creds.devpassword = ss->devpassword;
   creds.username    = ss->username;
   creds.password    = ss->password;
   creds.region      = ss->region;
   creds.language    = ss->language;

   system_id = screenscraper_system_id(entry->db_name
         ? entry->db_name : ss->system);

   if (!screenscraper_build_game_info_url(url, sizeof(url), &creds,
         system_id, crc32, rom_name, rom_size))
      return false;

   ss->flags &= ~(SS_SCRAPE_FLAG_HTTP_COMPLETE
                | SS_SCRAPE_FLAG_INFO_VALID);

   if (!(ss->http_task = (retro_task_t*)task_push_http_transfer(
         url, true, NULL, ss_scrape_info_cb, ss)))
      ss->flags |= SS_SCRAPE_FLAG_HTTP_COMPLETE;

   return true;
}

/* Builds the libretro thumbnail-server URL for the current entry and
 * classic media kind. Returns false when the kind has no equivalent on
 * the server (logos and all extended media). */
static bool ss_scrape_get_libretro_url(ss_scrape_handle_t *ss,
      enum screenscraper_media_kind kind, char *s, size_t len)
{
   char raw_url[PATH_MAX_LENGTH + 512];
   const char *system_name = NULL;
   const char *sub_dir     = NULL;
   const char *img_name    = NULL;

   switch (kind)
   {
      case SS_MEDIA_SNAP:
         sub_dir = "Named_Snaps";
         break;
      case SS_MEDIA_TITLE:
         sub_dir = "Named_Titles";
         break;
      case SS_MEDIA_BOXART:
         sub_dir = "Named_Boxarts";
         break;
      default:
         /* The libretro server hosts no logos or extended media */
         return false;
   }

   if (!ss->thumbnail_path_data)
      return false;
   if (*ss->thumbnail_path_data->content_db_name)
      system_name = ss->thumbnail_path_data->content_db_name;
   else if (*ss->thumbnail_path_data->system)
      system_name = ss->thumbnail_path_data->system;
   else
      return false;
   if (*ss->thumbnail_path_data->content_img)
      img_name = ss->thumbnail_path_data->content_img;
   else if (*ss->thumbnail_path_data->content_img_full)
      img_name = ss->thumbnail_path_data->content_img_full;
   else
      return false;

   snprintf(raw_url, sizeof(raw_url), "%s/%s/%s/%s",
         FILE_PATH_CORE_THUMBNAILS_URL, system_name, sub_dir, img_name);
   net_http_urlencode_full(s, raw_url, len);

   return !string_is_empty(s);
}

/* Computes the destination path of the current media kind (source
 * independent: both scrapers write the same file). */
static bool ss_scrape_current_dest(ss_scrape_handle_t *ss,
      char *path, size_t path_size)
{
   enum screenscraper_media_kind kind =
         (enum screenscraper_media_kind)ss->media_kind;
   const screenscraper_media_t *media = ss->game
         ? screenscraper_select_media(ss->game, kind, ss->region)
         : NULL;
   return ss_scrape_get_media_path(ss, media, kind, path, path_size);
}

/* Enqueues the download of the current media kind from the source
 * selected by 'attempt' (the ordered ScreenScraper/libretro-server
 * chain). Returns true if a transfer was started. */
static bool ss_scrape_request_media(ss_scrape_handle_t *ss)
{
   char path[PATH_MAX_LENGTH];
   char lr_url[2048];
   const char *urls[2];
   unsigned n_urls                    = 0;
   const screenscraper_media_t *media = NULL;
   file_transfer_t *transf            = NULL;
   enum screenscraper_media_kind kind =
         (enum screenscraper_media_kind)ss->media_kind;

   if (!ss->media_enabled[kind])
      return false;

   if (ss->game)
      media = screenscraper_select_media(ss->game, kind, ss->region);

   if (!ss_scrape_get_media_path(ss, media, kind, path, sizeof(path)))
      return false;

   /* Only download missing media unless overwriting */
   if (path_is_valid(path) && !ss->overwrite)
      return false;

   /* Assemble the ordered source chain */
   lr_url[0] = '\0';
   if (!ss_scrape_get_libretro_url(ss, kind, lr_url, sizeof(lr_url)))
      lr_url[0] = '\0';

   if (ss->prefer_libretro)
   {
      if (*lr_url)
         urls[n_urls++] = lr_url;
      if (media)
         urls[n_urls++] = media->url;
   }
   else
   {
      if (media)
         urls[n_urls++] = media->url;
      if (*lr_url)
         urls[n_urls++] = lr_url;
   }

   if (ss->attempt >= n_urls)
      return false;

   if (!(transf = (file_transfer_t*)malloc(sizeof(file_transfer_t))))
      return false;

   ss->flags        &= ~SS_SCRAPE_FLAG_HTTP_COMPLETE;

   transf->enum_idx  = MSG_UNKNOWN;
   transf->path[0]   = '\0';
   transf->user_data = (void*)ss;
   strlcpy(transf->path, path, sizeof(transf->path));

   if (!(ss->http_task = (retro_task_t*)task_push_http_transfer_file(
         urls[ss->attempt], true, NULL, ss_scrape_media_cb, transf)))
   {
      ss->flags |= SS_SCRAPE_FLAG_HTTP_COMPLETE;
      return false;
   }

   return true;
}

static void ss_scrape_free_handle(ss_scrape_handle_t *ss)
{
   if (!ss)
      return;

   if (ss->system)
      free(ss->system);
   if (ss->playlist_path)
      free(ss->playlist_path);
   if (ss->dir_thumbnails)
      free(ss->dir_thumbnails);
   if (ss->playlist)
      playlist_free(ss->playlist);
   if (ss->thumbnail_path_data)
      free(ss->thumbnail_path_data);
   if (ss->game)
      screenscraper_game_free(ss->game);

   free(ss);
}

/*************************/
/* Task Handler          */
/*************************/

static void task_screenscraper_handler(retro_task_t *task)
{
   uint8_t flg;
   ss_scrape_handle_t *ss = NULL;

   if (!task)
      goto task_finished;

   if (!(ss = (ss_scrape_handle_t*)task->state))
      goto task_finished;

   flg = task_get_flags(task);

   if ((flg & RETRO_TASK_FLG_CANCELLED) > 0)
   {
      /* Drain any in-flight transfer before freeing the handle: the
       * http callbacks dereference it and would otherwise write into
       * freed memory (heap corruption on cancel/exit). */
      if (   (ss->status == SS_SCRAPE_WAIT_INFO
           || ss->status == SS_SCRAPE_WAIT_MEDIA)
          && ss->http_task
          && !(ss->flags & SS_SCRAPE_FLAG_HTTP_COMPLETE))
         return;
      goto task_finished;
   }

   switch (ss->status)
   {
      case SS_SCRAPE_BEGIN:
         if (!path_is_valid(ss->playlist_config.path))
            goto task_finished;

         if (!(ss->playlist = playlist_init(&ss->playlist_config)))
            goto task_finished;

         ss->list_size = playlist_size(ss->playlist);
         if (ss->list_size < 1)
            goto task_finished;

         if (!(ss->thumbnail_path_data = gfx_thumbnail_path_init()))
            goto task_finished;

         if (!gfx_thumbnail_set_system(
               ss->thumbnail_path_data, ss->system, ss->playlist))
            goto task_finished;

         ss->status = SS_SCRAPE_ITERATE_ENTRY;
         break;

      case SS_SCRAPE_ITERATE_ENTRY:
         if (ss->list_index >= ss->list_size)
         {
            ss->status = SS_SCRAPE_END;
            break;
         }

         if (gfx_thumbnail_set_content_playlist(
               ss->thumbnail_path_data, ss->playlist, ss->list_index))
         {
            task_free_title(task);
            if (*ss->thumbnail_path_data->content_label)
               task_set_title(task,
                     strdup(ss->thumbnail_path_data->content_label));
            else
               task_set_title(task, strdup(""));
            task_set_progress(task,
                  (ss->list_index * 100) / ss->list_size);

            /* Allowance spent: either stop here and remember where to
             * pick up, or keep going with the libretro server alone */
            if (screenscraper_quota_exhausted())
            {
               if (!ss->quota_continue_libretro)
               {
                  ss->status = SS_SCRAPE_END;
                  break;
               }
               /* Libretro-only pass: no lookup, media step still runs
                * and falls back to the thumbnail server */
               if (ss->game)
               {
                  screenscraper_game_free(ss->game);
                  ss->game = NULL;
               }
               ss->media_kind = 0;
               ss->attempt    = 0;
               ss->status     = SS_SCRAPE_ITERATE_MEDIA;
               break;
            }

            if (ss_scrape_request_info(ss))
               ss->status = SS_SCRAPE_WAIT_INFO;
            else
            {
               /* Cannot query this entry (or credentials are
                * missing) - skip ahead */
               ss->list_index++;
            }
         }
         else
            ss->list_index++;
         break;

      case SS_SCRAPE_WAIT_INFO:
         if (!ss->http_task)
            ss->flags |= SS_SCRAPE_FLAG_HTTP_COMPLETE;

         if (!(ss->flags & SS_SCRAPE_FLAG_HTTP_COMPLETE))
            break;

         ss->http_task = NULL;

         if (ss->flags & SS_SCRAPE_FLAG_INFO_VALID)
            ss_scrape_write_metadata(ss);

         /* Iterate media even when the ScreenScraper lookup missed:
          * the libretro thumbnail server can still provide the classic
          * thumbnail types as a fallback source */
         ss->media_kind = 0;
         ss->attempt    = 0;
         ss->status     = SS_SCRAPE_ITERATE_MEDIA;
         break;

      case SS_SCRAPE_ITERATE_MEDIA:
         if (ss->media_kind >= SS_MEDIA_LAST)
         {
            /* Entry complete */
            if (ss->game)
            {
               screenscraper_game_free(ss->game);
               ss->game = NULL;
            }
            ss->list_index++;
            ss->status = ss->single_entry
                  ? SS_SCRAPE_END : SS_SCRAPE_ITERATE_ENTRY;
            break;
         }

         if (ss_scrape_request_media(ss))
            ss->status = SS_SCRAPE_WAIT_MEDIA;
         else
         {
            ss->media_kind++;
            ss->attempt = 0;
         }
         break;

      case SS_SCRAPE_WAIT_MEDIA:
         if (!ss->http_task)
            ss->flags |= SS_SCRAPE_FLAG_HTTP_COMPLETE;

         if (!(ss->flags & SS_SCRAPE_FLAG_HTTP_COMPLETE))
            break;

         ss->http_task = NULL;

         {
            char dest[PATH_MAX_LENGTH];
            /* When the download failed, move on to the next source of
             * the chain; the request logic advances the kind once the
             * chain is exhausted */
            if (   ss_scrape_current_dest(ss, dest, sizeof(dest))
                && !path_is_valid(dest))
               ss->attempt++;
            else
            {
               ss->media_kind++;
               ss->attempt = 0;
            }
         }
         ss->status = SS_SCRAPE_ITERATE_MEDIA;
         break;

      case SS_SCRAPE_END:
      default:
         task_set_progress(task, 100);
         goto task_finished;
   }

   return;

task_finished:
   if (task)
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);

   if (ss && !ss->single_entry)
   {
      if (   screenscraper_quota_exhausted()
          && !ss->quota_continue_libretro
          && ss->list_index < ss->list_size)
      {
         /* Paused part way: remember the spot and say so, once */
         char msg[128];
         size_t _len = strlcpy(msg,
               msg_hash_to_str(MSG_SCREENSCRAPER_QUOTA_PAUSED),
               sizeof(msg));
         ss_scrape_resume_save(ss);
         runloop_msg_queue_push(msg, _len, 2, 300, true, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }
      else if (ss->list_index >= ss->list_size)
         ss_scrape_resume_clear(ss->dir_thumbnails);
   }

   ss_scrape_free_handle(ss);
   if (task)
      task->state = NULL;
}

static bool task_screenscraper_finder(retro_task_t *task, void *user_data)
{
   ss_scrape_handle_t *ss = NULL;

   if (!task || !user_data)
      return false;
   if (task->handler != task_screenscraper_handler)
      return false;
   if (!(ss = (ss_scrape_handle_t*)task->state))
      return false;

   return string_is_equal(
         (const char*)user_data, ss->playlist_config.path);
}

/*************************/
/* Task Push             */
/*************************/

static void ss_scrape_copy_settings(ss_scrape_handle_t *ss,
      settings_t *settings);

bool task_push_pl_screenscraper(
      const char *system,
      const playlist_config_t *playlist_config,
      const char *dir_thumbnails)
{
   return task_push_pl_screenscraper_at(system, playlist_config,
         dir_thumbnails, 0);
}

/* Same, but begins at 'start_index' - used to resume a scrape that the
 * request allowance interrupted. */
bool task_push_pl_screenscraper_at(
      const char *system,
      const playlist_config_t *playlist_config,
      const char *dir_thumbnails,
      size_t start_index)
{
   task_finder_data_t find_data;
   settings_t *settings   = config_get_ptr();
   retro_task_t *task     = NULL;
   ss_scrape_handle_t *ss = NULL;

   if (   !settings
       || !playlist_config
       || string_is_empty(system)
       || string_is_empty(playlist_config->path)
       || string_is_empty(dir_thumbnails))
      return false;

   /* Concurrent scrapes of the same playlist make no sense */
   find_data.func     = task_screenscraper_finder;
   find_data.userdata = (void*)playlist_config->path;
   if (task_queue_find(&find_data))
      return false;

   if (!(task = task_init()))
      return false;

   if (!(ss = (ss_scrape_handle_t*)calloc(1, sizeof(*ss))))
   {
      free(task);
      return false;
   }

   ss->system         = strdup(system);
   ss->playlist_path  = strdup(playlist_config->path);
   ss->dir_thumbnails = strdup(dir_thumbnails);
   ss->status         = SS_SCRAPE_BEGIN;
   ss->list_index     = start_index;

   if (!playlist_config_copy(playlist_config, &ss->playlist_config))
   {
      ss_scrape_free_handle(ss);
      free(task);
      return false;
   }

   ss_scrape_copy_settings(ss, settings);

   task->handler      = task_screenscraper_handler;
   task->state        = ss;
   task->title        = strdup(system);
   task->progress     = 0;
   task->progress_cb  = task_window_progress_cb;
   task->flags       |= RETRO_TASK_FLG_ALTERNATIVE_LOOK;

   task_queue_push(task);

   return true;
}

/* Copies the scraper-relevant settings into the handle, so mid-scrape
 * changes cannot corrupt the run */
static void ss_scrape_copy_settings(ss_scrape_handle_t *ss,
      settings_t *settings)
{
   strlcpy(ss->username, settings->arrays.screenscraper_username,
         sizeof(ss->username));
   strlcpy(ss->password, settings->arrays.screenscraper_password,
         sizeof(ss->password));
   strlcpy(ss->devid, settings->arrays.screenscraper_devid,
         sizeof(ss->devid));
   strlcpy(ss->devpassword, settings->arrays.screenscraper_devpassword,
         sizeof(ss->devpassword));
   strlcpy(ss->region,
         screenscraper_region_code(settings->uints.screenscraper_region),
         sizeof(ss->region));
   strlcpy(ss->language,
         screenscraper_language_code(settings->uints.screenscraper_language),
         sizeof(ss->language));

   ss->media_enabled[SS_MEDIA_BOXART]    =
         settings->bools.screenscraper_media_boxarts;
   ss->media_enabled[SS_MEDIA_SNAP]      =
         settings->bools.screenscraper_media_snaps;
   ss->media_enabled[SS_MEDIA_TITLE]     =
         settings->bools.screenscraper_media_titles;
   ss->media_enabled[SS_MEDIA_LOGO]      =
         settings->bools.screenscraper_media_logos;
   ss->media_enabled[SS_MEDIA_BOXART_3D] =
         settings->bools.screenscraper_media_boxarts3d;
   ss->media_enabled[SS_MEDIA_FANART]    =
         settings->bools.screenscraper_media_fanarts;
   ss->media_enabled[SS_MEDIA_MARQUEE]   =
         settings->bools.screenscraper_media_marquees;
   ss->media_enabled[SS_MEDIA_VIDEO]     =
         settings->bools.screenscraper_media_videos;
   ss->media_enabled[SS_MEDIA_MANUAL]    =
         settings->bools.screenscraper_media_manuals;
   ss->media_enabled[SS_MEDIA_BEZEL]     =
         settings->bools.screenscraper_media_bezels;
   ss->metadata_enabled                  =
         settings->bools.screenscraper_metadata;
   ss->overwrite                         =
         settings->bools.screenscraper_overwrite;
   ss->use_crc                           =
         settings->bools.screenscraper_use_crc;
   ss->prefer_libretro                   =
         (settings->uints.screenscraper_primary_scraper == 1);
   ss->quota_continue_libretro           =
         (settings->uints.screenscraper_quota_action == 1);
}

/*************************/
/* Resume after a pause  */
/*************************/

typedef struct
{
   char system[NAME_MAX_LENGTH];
   char playlist_path[PATH_MAX_LENGTH];
   char dir_thumbnails[PATH_MAX_LENGTH];
   size_t index;
} ss_resume_t;

/* ssuserInfos.php callback: resume the saved scrape when the account
 * has requests left again. */
static void ss_resume_user_info_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   http_transfer_data_t *data = (http_transfer_data_t*)task_data;
   ss_resume_t *resume        = (ss_resume_t*)user_data;
   screenscraper_quota_t quota;
   settings_t *settings       = config_get_ptr();

   if (!resume)
      return;

   if (   !data || !data->data || data->status != 200
       || !screenscraper_parse_user_info(data->data, data->len, &quota))
   {
      RARCH_WARN("[ScreenScraper] Could not read account status; "
            "the paused scrape stays paused.\n");
      goto finish;
   }

   RARCH_LOG("[ScreenScraper] Account allowance: %u/%u requests used.\n",
         quota.used, quota.max);

   if (quota.max > 0 && quota.used >= quota.max)
   {
      screenscraper_set_quota_exhausted(true);
      RARCH_LOG("[ScreenScraper] Allowance still spent; "
            "scrape of \"%s\" remains paused at entry %u.\n",
            resume->system, (unsigned)resume->index);
      goto finish;
   }

   /* Headroom again: pick the scrape up where it stopped */
   screenscraper_set_quota_exhausted(false);

   if (settings)
   {
      playlist_config_t cfg;
      cfg.capacity            = COLLECTION_SIZE;
      cfg.old_format          = settings->bools.playlist_use_old_format;
      cfg.compress            = settings->bools.playlist_compression;
      cfg.fuzzy_archive_match = settings->bools.playlist_fuzzy_archive_match;
      cfg.autofix_paths       = false;
      cfg.path[0]             = '\0';
      cfg.base_content_directory[0] = '\0';
      playlist_config_set_path(&cfg, resume->playlist_path);

      if (task_push_pl_screenscraper_at(resume->system, &cfg,
            resume->dir_thumbnails, resume->index))
      {
         char msg[128];
         size_t _len = strlcpy(msg,
               msg_hash_to_str(MSG_SCREENSCRAPER_QUOTA_RESUMED),
               sizeof(msg));
         RARCH_LOG("[ScreenScraper] Resuming \"%s\" at entry %u.\n",
               resume->system, (unsigned)resume->index);
         runloop_msg_queue_push(msg, _len, 2, 300, true, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }
   }

finish:
   free(resume);
}

/* Called once at startup: if a scrape was paused by the request
 * allowance, ask the service whether there is headroom again and, if
 * so, carry on from the saved entry. */
bool task_push_screenscraper_resume_check(void)
{
   char path[PATH_MAX_LENGTH];
   char url[2048];
   char *file_buf         = NULL;
   int64_t file_len       = 0;
   settings_t *settings   = config_get_ptr();
   ss_resume_t *resume    = NULL;
   screenscraper_creds_t creds;
   struct string_list *lines = NULL;

   if (!settings || !screenscraper_signed_in())
      return false;
   if (string_is_empty(settings->paths.directory_thumbnails))
      return false;

   ss_scrape_resume_path(settings->paths.directory_thumbnails,
         path, sizeof(path));
   if (!path_is_valid(path))
      return false;

   if (!filestream_read_file(path, (void**)&file_buf, &file_len)
       || !file_buf)
      return false;

   lines = string_split(file_buf, "\n");
   free(file_buf);

   if (!lines || lines->size < 3)
   {
      if (lines)
         string_list_free(lines);
      return false;
   }

   if (!(resume = (ss_resume_t*)calloc(1, sizeof(*resume))))
   {
      string_list_free(lines);
      return false;
   }

   strlcpy(resume->system, lines->elems[0].data, sizeof(resume->system));
   resume->index = (size_t)strtoul(lines->elems[1].data, NULL, 10);
   strlcpy(resume->playlist_path, lines->elems[2].data,
         sizeof(resume->playlist_path));
   strlcpy(resume->dir_thumbnails, settings->paths.directory_thumbnails,
         sizeof(resume->dir_thumbnails));
   string_list_free(lines);

   if (   string_is_empty(resume->system)
       || string_is_empty(resume->playlist_path)
       || !path_is_valid(resume->playlist_path))
   {
      free(resume);
      return false;
   }

   creds.devid       = settings->arrays.screenscraper_devid;
   creds.devpassword = settings->arrays.screenscraper_devpassword;
   creds.username    = settings->arrays.screenscraper_username;
   creds.password    = settings->arrays.screenscraper_password;
   creds.region      = screenscraper_region_code(
         settings->uints.screenscraper_region);
   creds.language    = screenscraper_language_code(
         settings->uints.screenscraper_language);

   if (!screenscraper_build_user_info_url(url, sizeof(url), &creds))
   {
      free(resume);
      return false;
   }

   RARCH_LOG("[ScreenScraper] Paused scrape found; checking allowance.\n");

   if (!task_push_http_transfer(url, true, NULL,
         ss_resume_user_info_cb, resume))
   {
      free(resume);
      return false;
   }

   return true;
}

/* On-demand variant: scrapes a single playlist entry. Lets artwork
 * appear while browsing, using ScreenScraper's hash/name matching
 * rather than the libretro server's exact-label lookup. */
bool task_push_pl_entry_screenscraper(
      const char *system,
      playlist_t *playlist,
      unsigned idx)
{
   settings_t *settings   = config_get_ptr();
   const char *conf_path  = NULL;
   retro_task_t *task     = NULL;
   ss_scrape_handle_t *ss = NULL;
   playlist_config_t cfg;

   if (   !settings || !playlist
       || string_is_empty(system)
       || !screenscraper_signed_in())
      return false;
   if (string_is_empty(settings->paths.directory_thumbnails))
      return false;
   if (!(conf_path = playlist_get_conf_path(playlist))
       || string_is_empty(conf_path))
      return false;

   cfg.capacity            = COLLECTION_SIZE;
   cfg.old_format          = settings->bools.playlist_use_old_format;
   cfg.compress            = settings->bools.playlist_compression;
   cfg.fuzzy_archive_match = settings->bools.playlist_fuzzy_archive_match;
   cfg.autofix_paths       = false;
   cfg.path[0]             = '\0';
   cfg.base_content_directory[0] = '\0';
   playlist_config_set_path(&cfg, conf_path);

   /* One in-flight scrape per playlist is plenty; while a scrape of
    * this playlist is already running, further on-demand requests are
    * simply dropped */
   {
      task_finder_data_t find_data;
      find_data.func     = task_screenscraper_finder;
      find_data.userdata = (void*)cfg.path;
      if (task_queue_find(&find_data))
         return false;
   }

   if (!(task = task_init()))
      return false;

   if (!(ss = (ss_scrape_handle_t*)calloc(1, sizeof(*ss))))
   {
      free(task);
      return false;
   }

   ss->system         = strdup(system);
   ss->playlist_path  = strdup(conf_path);
   ss->dir_thumbnails = strdup(settings->paths.directory_thumbnails);
   ss->status         = SS_SCRAPE_BEGIN;
   ss->single_entry   = true;
   ss->list_index     = idx;

   if (!playlist_config_copy(&cfg, &ss->playlist_config))
   {
      ss_scrape_free_handle(ss);
      free(task);
      return false;
   }

   ss_scrape_copy_settings(ss, settings);

   task->handler  = task_screenscraper_handler;
   task->state    = ss;
   task->title    = strdup(system);
   task->progress = 0;
   task->flags   |= RETRO_TASK_FLG_MUTE;

   task_queue_push(task);
   return true;
}
