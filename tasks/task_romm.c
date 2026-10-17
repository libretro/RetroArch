/*  RetroArch - A frontend for libretro.
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

/* RomM (https://romm.app) as a content source: list its platforms and
 * ROMs, download a ROM, add it to the playlist of its system and fetch
 * its cover as the boxart thumbnail. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <file/archive_file.h>
#include <file/file_path.h>
#include <lists/string_list.h>
#include <streams/file_stream.h>
#include <net/net_http.h>
#include <string/stdstring.h>

#include "tasks_internal.h"
#include "task_file_transfer.h"
#include "task_romm.h"

#include "../configuration.h"
#include "../file_path_special.h"
#include "../msg_hash.h"
#include "../playlist.h"
#include "../verbosity.h"
#include "../runloop.h"
#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#endif

typedef struct romm_dl
{
   char rom_path[PATH_MAX_LENGTH];
   char part_path[PATH_MAX_LENGTH];
   char thumb_path[PATH_MAX_LENGTH];
   char playlist_path[PATH_MAX_LENGTH];
   char cover_url[PATH_MAX_LENGTH];
   char header[NAME_MAX_LENGTH + 32];
   char label[NAME_MAX_LENGTH];
   char db_name[NAME_MAX_LENGTH + 8];
   char dir[DIR_MAX_LENGTH];
   char url[DIR_MAX_LENGTH];
   /* A multi-file ROM arrives as a zip, unpacked into out_dir */
   char zip_path[PATH_MAX_LENGTH];
   char out_dir[DIR_MAX_LENGTH];
   char tmp_dir[DIR_MAX_LENGTH];
   char target[DIR_MAX_LENGTH];
   char m3u_name[NAME_MAX_LENGTH + 8];
   char *m3u;
   struct string_list *members;
   bool multi;
} romm_dl_t;

typedef struct romm_playlist
{
   playlist_parse_t *parse;
   playlist_t *playlist;
   romm_dl_t *dl;
   playlist_config_t config; /* size_t alignment */
   unsigned seq;
   bool created;
} romm_playlist_t;

/* Pushes are ordered, so two ROMs of one system cannot overwrite each
 * other's playlist write */
static unsigned romm_playlist_seq;

static void romm_dl_free(romm_dl_t *dl)
{
   if (dl)
   {
      free(dl->m3u);
      if (dl->members)
         string_list_free(dl->members);
   }
   free(dl);
}

static bool romm_base_url(settings_t *settings, char *s, size_t len)
{
   size_t n;
   const char *url = settings->arrays.romm_url;

   if (!string_starts_with(url, "http://") && !string_starts_with(url, "https://"))
      return false;
   if ((n = strlcpy(s, url, len)) >= len)
      return false;
   while (n > 0 && s[n - 1] == '/')
      s[--n] = '\0';
   return true;
}

static void romm_auth_header(settings_t *settings, char *s, size_t len)
{
   size_t _len;

   s[0] = '\0';
   if (!*settings->arrays.romm_token)
      return;
   _len  = strlcpy(s, "Authorization: Bearer ", len);
   _len += strlcpy(s + _len, settings->arrays.romm_token, len - _len);
   strlcpy(s + _len, "\r\n", len - _len);
}

void *task_push_romm_list(const char *list_ctx,
      retro_task_callback_t cb, void *user_data)
{
   unsigned offset, id;
   char url[DIR_MAX_LENGTH];
   char header[NAME_MAX_LENGTH + 32];
   static bool warned;
   settings_t *settings = config_get_ptr();
   size_t _len;

   if (!romm_base_url(settings, url, sizeof(url)))
      return NULL;
   _len = strlen(url);

   if (   !warned && *settings->arrays.romm_token
       && string_starts_with(url, "http://"))
   {
      warned = true;
      RARCH_WARN("[RomM] The API token is sent unencrypted to %s;"
            " use an https:// address unless the network is trusted.\n", url);
   }

   if (list_ctx && *list_ctx)
   {
      if (!romm_list_ctx_split(list_ctx, &offset, &id, NULL, 0, NULL, 0))
         return NULL;
      snprintf(url + _len, sizeof(url) - _len,
            "/api/roms?platform_ids=%u&limit=%u&offset=%u&order_by=name"
            "&with_char_index=false&with_filter_values=false"
            "&with_rom_id_index=false",
            id, ROMM_PAGE_SIZE, offset);
   }
   else
      strlcpy(url + _len, "/api/platforms", sizeof(url) - _len);

   romm_auth_header(settings, header, sizeof(header));
   return task_push_http_transfer_with_headers(url, true, NULL,
         *header ? header : NULL, cb, user_data);
}

/* Playlist */

static void task_romm_playlist_handler(retro_task_t *task);

static bool romm_within_budget(void *ud)
{
   return task_nbio_slice_within_budget(ud, 0, 0);
}

/* True when an earlier push for the same playlist is still running */
static bool romm_playlist_older_finder(retro_task_t *task, void *user_data)
{
   const romm_playlist_t *self = (const romm_playlist_t*)user_data;
   const romm_playlist_t *other;

   if (task->handler != task_romm_playlist_handler)
      return false;
   if (!(other = (const romm_playlist_t*)task->state) || other == self)
      return false;
   return (int)(other->seq - self->seq) < 0
      && !(task_get_flags(task) & RETRO_TASK_FLG_FINISHED)
      && string_is_equal(other->config.path, self->config.path);
}

static void task_romm_playlist_handler(retro_task_t *task)
{
   task_finder_data_t find_data;
   romm_playlist_t *st = (romm_playlist_t*)task->state;

   if (!st || (task_get_flags(task) & RETRO_TASK_FLG_CANCELLED))
      goto finished;

   if (!st->playlist && !st->parse)
   {
      find_data.func     = romm_playlist_older_finder;
      find_data.userdata = st;
      if (task_queue_find(&find_data))
      {
         task_queue_park(task, 50000);
         return;
      }

      if (path_is_valid(st->config.path))
      {
         if (!(st->parse = playlist_parse_begin(&st->config)))
            goto finished;
      }
      else
      {
         st->created = true;
         if (!(st->playlist = playlist_init(&st->config)))
            goto finished;
      }
   }

   if (st->parse)
   {
      nbio_budget_t b;
      int r;

      task_nbio_slice_open(&b);
      r = playlist_parse_step(st->parse, romm_within_budget, &b);
      task_nbio_slice_close(&b);

      if (r == 0)
         return;
      if (r < 0)
      {
         playlist_parse_abort(st->parse);
         st->parse = NULL;
         goto finished;
      }
      st->playlist = playlist_parse_end(st->parse);
      st->parse    = NULL;
      if (!st->playlist)
         goto finished;
   }

   {
      struct playlist_entry entry = {0};

      entry.path      = st->dl->rom_path;
      entry.label     = st->dl->label;
      entry.core_path = (char*)FILE_PATH_DETECT;
      entry.core_name = (char*)FILE_PATH_DETECT;
      entry.crc32     = (char*)"00000000|crc";
      entry.db_name   = st->dl->db_name;
      PLAYLIST_SET_ENTRY_SLOT(&entry, 0);

      if (playlist_push(st->playlist, &entry))
         playlist_write_file(st->playlist);
   }

finished:
   task_set_progress(task, 100);
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void cb_task_romm_playlist(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   romm_playlist_t *st;
   playlist_t *cached = playlist_get_cached();
#ifdef HAVE_MENU
   struct menu_state *menu_st = menu_state_get_ptr();
#endif

   if (!task || !(st = (romm_playlist_t*)task->state))
      return;

   /* The write only flags the menu's copy stale, and the menu would go
    * on editing and saving it over this entry: read it again now, as
    * the scanner and the playlist manager do */
   if (cached && string_is_equal(st->config.path, playlist_get_conf_path(cached)))
   {
      playlist_config_t *config = (playlist_config_t*)malloc(sizeof(*config));

      if (config && playlist_config_copy(playlist_get_config(cached), config))
      {
         playlist_free_cached();
         playlist_init_cached(config);
      }
      free(config);
   }

#ifdef HAVE_MENU
   if (st->created && menu_st->driver_ctx && menu_st->driver_ctx->environ_cb)
      menu_st->driver_ctx->environ_cb(MENU_ENVIRON_RESET_HORIZONTAL_LIST,
            NULL, menu_st->userdata);
#endif
}

static void task_romm_playlist_free(retro_task_t *task)
{
   romm_playlist_t *st = (romm_playlist_t*)task->state;

   if (!st)
      return;
   if (st->parse)
      playlist_parse_abort(st->parse);
   if (st->playlist)
      playlist_free(st->playlist);
   romm_dl_free(st->dl);
   free(st);
}

/* Takes ownership of @dl on success */
static bool romm_playlist_push(romm_dl_t *dl)
{
   settings_t *settings = config_get_ptr();
   retro_task_t *task   = task_init();
   romm_playlist_t *st  = (romm_playlist_t*)calloc(1, sizeof(*st));

   if (!task || !st)
      goto error;

   st->dl                          = dl;
   st->seq                         = ++romm_playlist_seq;
   st->config.capacity             = COLLECTION_SIZE;
   st->config.old_format           = settings->bools.playlist_use_old_format;
   st->config.compress             = settings->bools.playlist_compression;
   st->config.fuzzy_archive_match  = settings->bools.playlist_fuzzy_archive_match;
   playlist_config_set_base_content_directory(&st->config,
         settings->bools.playlist_portable_paths
         ? settings->paths.directory_menu_content : NULL);
   playlist_config_set_path(&st->config, dl->playlist_path);

   task->handler  = task_romm_playlist_handler;
   task->state    = st;
   task->title    = strdup(dl->label);
   task->callback = cb_task_romm_playlist;
   task->cleanup  = task_romm_playlist_free;
   task->flags   |= RETRO_TASK_FLG_ALTERNATIVE_LOOK | RETRO_TASK_FLG_MUTE;

   task_queue_push(task);
   return true;

error:
   free(task);
   free(st);
   return false;
}

/* Download */

/* A file the user already has is only replaced by a complete one: the
 * transfer writes beside it and this renames over it when it is done.
 * @paths holds the final path, a NUL, then the one written. */
static void cb_romm_cover(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   http_transfer_data_t *data = (http_transfer_data_t*)task_data;
   char *paths                = (char*)user_data;
   bool ok                    = !err && data
      && data->status >= 200 && data->status < 300;

   if (!(ok && filestream_rename(paths + strlen(paths) + 1, paths) == 0))
      filestream_delete(paths + strlen(paths) + 1);
   free(paths);
}

static void romm_notify(enum msg_hash_enums msg)
{
   const char *s = msg_hash_to_str(msg);

   runloop_msg_queue_push(s, strlen(s), 1, 100, true, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
}

/* The ROM is on disk where the playlist will point: fetch its cover and
 * add it to the playlist. Takes ownership of @dl. */
static void romm_finish(romm_dl_t *dl)
{
   if (*dl->thumb_path && !path_is_valid(dl->thumb_path))
   {
      size_t n    = strlen(dl->thumb_path);
      char *paths = (char*)malloc(2 * n + 8);

      fill_pathname_basedir(dl->dir, dl->thumb_path, sizeof(dl->dir));
      if (paths && path_mkdir(dl->dir))
      {
         memcpy(paths, dl->thumb_path, n + 1);
         memcpy(paths + n + 1, dl->thumb_path, n);
         strlcpy(paths + 2 * n + 1, ".part", 6);
         if (path_is_valid(paths + n + 1))
            free(paths);
         else if (!task_push_http_download_file_with_headers(dl->cover_url,
                  paths + n + 1, *dl->header ? dl->header : NULL,
                  true, NULL, cb_romm_cover, paths))
            free(paths);
      }
      else
         free(paths);
   }

   if (!romm_playlist_push(dl))
      romm_dl_free(dl);
}

#ifdef HAVE_COMPRESSION
/* A set bigger than this, or with more members, is not a game */
#define ROMM_EXTRACT_MAX_ENTRIES 1024
#define ROMM_EXTRACT_MAX_BYTES   ((uint64_t)8 << 30)

typedef struct romm_scan
{
   struct archive_extract_userdata userdata;
   file_archive_transfer_t transfer;
   struct string_list *names;
   uint64_t total;
   bool bad;
} romm_scan_t;

/* Refuses the whole archive over one member that could leave the target
 * directory or that makes the set too large, rather than skipping it */
static int romm_scan_cb(const char *name, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode, uint32_t csize, uint32_t size,
      uint32_t crc32, struct archive_extract_userdata *userdata)
{
   union string_list_elem_attr attr;
   romm_scan_t *scan = (romm_scan_t*)userdata->cb_data;

   scan->total += size;
   attr.i       = (int)size;
   if (   romm_entry_is_safe(name)
       && scan->total <= ROMM_EXTRACT_MAX_BYTES
       && scan->names->size < ROMM_EXTRACT_MAX_ENTRIES
       && string_list_append(scan->names, name, attr))
      return 1;
   scan->bad = true;
   return 0;
}

/* The member names and sizes of the zip, read from its directory.
 * NULL unless every member is acceptable. */
static struct string_list *romm_scan(const romm_dl_t *dl)
{
   bool returnerr            = true;
   struct string_list *names = NULL;
   romm_scan_t *scan         = (romm_scan_t*)calloc(1, sizeof(*scan));

   if (!scan)
      return NULL;
   if (!(scan->names = string_list_new()))
   {
      free(scan);
      return NULL;
   }
   scan->transfer.type    = ARCHIVE_TRANSFER_INIT;
   scan->userdata.cb_data = scan;
   strlcpy(scan->userdata.archive_path, dl->zip_path,
         sizeof(scan->userdata.archive_path));

   while (file_archive_parse_file_iterate(&scan->transfer, &returnerr,
            dl->zip_path, NULL, romm_scan_cb, &scan->userdata) == 0)
      ;

   if (returnerr && !scan->bad && scan->names->size)
      names = scan->names;
   else
      string_list_free(scan->names);
   free(scan);
   return names;
}

/* Takes back what the unpack made: the members by name, then their
 * folders deepest-first, then the folder itself. The folder is one this
 * task created and the zip reader makes no links, so nothing here can
 * be led out of it. Anything else found inside is left, and so is the
 * folder. */
static void romm_unpack_failed(romm_dl_t *dl)
{
   size_t i;
   char *slash;

   for (i = 0; dl->members && i < dl->members->size; i++)
   {
      const char *name = dl->members->elems[i].data;

      if (name[strlen(name) - 1] == '/')
         continue;
      fill_pathname_join_special(dl->rom_path, dl->tmp_dir, name,
            sizeof(dl->rom_path));
      filestream_delete(dl->rom_path);
   }
   if (dl->m3u)
   {
      fill_pathname_join_special(dl->rom_path, dl->tmp_dir, dl->target,
            sizeof(dl->rom_path));
      filestream_delete(dl->rom_path);
   }
   for (i = 0; dl->members && i < dl->members->size; i++)
   {
      strlcpy(dl->url, dl->members->elems[i].data, sizeof(dl->url));
      if (dl->url[strlen(dl->url) - 1] == '/')
         dl->url[strlen(dl->url) - 1] = '\0';
      else if ((slash = strrchr(dl->url, '/')))
         *slash = '\0';
      else
         continue;
      while (*dl->url)
      {
         fill_pathname_join_special(dl->rom_path, dl->tmp_dir, dl->url,
               sizeof(dl->rom_path));
         path_rmdir(dl->rom_path);
         if (!(slash = strrchr(dl->url, '/')))
            break;
         *slash = '\0';
      }
   }
   path_rmdir(dl->tmp_dir);
   romm_notify(MSG_DECOMPRESSION_FAILED);
   romm_dl_free(dl);
}

/* The set was unpacked into a folder of its own beside the final one,
 * which is known not to exist: one rename puts it in place */
static void cb_romm_extract(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   decompress_task_data_t *data = (decompress_task_data_t*)task_data;
   romm_dl_t *dl                = (romm_dl_t*)user_data;

   if (data)
   {
      free(data->source_file);
      free(data);
   }
   filestream_delete(dl->zip_path);

   if (err)
   {
      RARCH_ERR("[RomM] Unpacking \"%s\" failed: %s.\n", dl->label, err);
      romm_unpack_failed(dl);
      return;
   }

   if (dl->m3u)
   {
      fill_pathname_join_special(dl->rom_path, dl->tmp_dir, dl->target,
            sizeof(dl->rom_path));
      if (!filestream_write_file(dl->rom_path, dl->m3u, strlen(dl->m3u)))
      {
         RARCH_ERR("[RomM] Could not write \"%s\".\n", dl->rom_path);
         romm_unpack_failed(dl);
         return;
      }
      free(dl->m3u);
      dl->m3u = NULL;
   }

   if (path_is_valid(dl->out_dir) || filestream_rename(dl->tmp_dir, dl->out_dir))
   {
      /* Nothing is deleted: the unpacked set stays where it is */
      RARCH_ERR("[RomM] Could not move \"%s\" into place; it is in \"%s\".\n",
            dl->label, dl->tmp_dir);
      romm_notify(MSG_ROMM_MOVE_FAILED);
      romm_dl_free(dl);
      return;
   }

   fill_pathname_join_special(dl->rom_path, dl->out_dir, dl->target,
         sizeof(dl->rom_path));
   pathname_conform_slashes_to_os(dl->rom_path);

   RARCH_LOG("[RomM] Unpacked \"%s\".\n", dl->label);
   romm_finish(dl);
}
#endif

/* Unpacks the zip of a multi-file ROM beside it and picks what the
 * playlist gets. Takes ownership of @dl. */
static void romm_extract(romm_dl_t *dl)
{
   bool ok = false;
#ifdef HAVE_COMPRESSION
   struct string_list *list = romm_scan(dl);

   if (list)
   {
      size_t i;
      const char **names = (const char**)malloc(list->size * sizeof(*names));
      uint32_t *sizes    = (uint32_t*)malloc(list->size * sizeof(*sizes));

      if (names && sizes)
      {
         for (i = 0; i < list->size; i++)
         {
            names[i] = list->elems[i].data;
            sizes[i] = (uint32_t)list->elems[i].attr.i;
         }
         ok = romm_archive_target(names, sizes, list->size, dl->m3u_name,
               dl->target, sizeof(dl->target), &dl->m3u);
         if (ok && path_is_valid(dl->tmp_dir))
         {
            /* Put there since the download began: not ours to unpack
             * into or to clean up */
            RARCH_ERR("[RomM] \"%s\" is in the way.\n", dl->tmp_dir);
            romm_notify(MSG_ROMM_LEFTOVER_EXISTS);
            filestream_delete(dl->zip_path);
            string_list_free(list);
            list = NULL;
            free(names);
            free(sizes);
            romm_dl_free(dl);
            return;
         }
         if (ok)
         {
            ok = task_push_decompress(dl->zip_path, dl->tmp_dir, NULL, NULL,
                  NULL, cb_romm_extract, dl, NULL, false) != NULL;
         }
      }
      free(names);
      free(sizes);
      if (ok)
         dl->members = list;
      else
         string_list_free(list);
   }
#endif

   if (!ok)
   {
      RARCH_ERR("[RomM] Cannot unpack \"%s\".\n", dl->label);
      filestream_delete(dl->zip_path);
      romm_notify(MSG_DECOMPRESSION_FAILED);
      romm_dl_free(dl);
   }
}

static void cb_romm_download(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   http_transfer_data_t *data = (http_transfer_data_t*)task_data;
   romm_dl_t *dl              = (romm_dl_t*)user_data;

   if (!dl)
      return;

   if (err || !data || data->status < 200 || data->status > 299)
   {
      RARCH_ERR("[RomM] Download of \"%s\" failed: %s.\n", dl->label,
            err ? err : "bad status");
      romm_dl_free(dl);
      return;
   }

   RARCH_LOG("[RomM] Downloaded \"%s\".\n", dl->label);

   if (dl->multi && path_is_valid(dl->zip_path))
   {
      RARCH_ERR("[RomM] \"%s\" is in the way.\n", dl->zip_path);
      romm_notify(MSG_ROMM_LEFTOVER_EXISTS);
      filestream_delete(dl->part_path);
      romm_dl_free(dl);
   }
   else if (filestream_rename(dl->part_path,
            dl->multi ? dl->zip_path : dl->rom_path))
   {
      RARCH_ERR("[RomM] Could not move \"%s\" into place.\n", dl->label);
      filestream_delete(dl->part_path);
      romm_dl_free(dl);
   }
   else if (dl->multi)
      romm_extract(dl);
   else
      romm_finish(dl);
}

/* Same replacements as the thumbnail names the menu looks up */
static void romm_thumbnail_name(char *s)
{
   char *c;
   while ((c = strpbrk(s, "&*/:`\"<>?\\|")))
      *c = '_';
}

void task_push_romm_download(const char *list_ctx, const char *rom_ctx)
{
   unsigned offset, platform_id, rom_id;
   enum msg_hash_enums notify;
   bool multi;
   char system[NAME_MAX_LENGTH];
   char fs_slug[NAME_MAX_LENGTH];
   char cover[NAME_MAX_LENGTH + 8];
   char fs_name[NAME_MAX_LENGTH];
   char *encoded        = NULL;
   romm_dl_t *dl        = NULL;
   settings_t *settings = config_get_ptr();
   size_t base_len, _len;

   if (   !romm_list_ctx_split(list_ctx, &offset, &platform_id,
            system, sizeof(system), fs_slug, sizeof(fs_slug))
       || !romm_rom_ctx_split(rom_ctx, &rom_id, &multi,
            cover, sizeof(cover), fs_name, sizeof(fs_name))
       || !*settings->paths.directory_core_assets
       || !*settings->paths.directory_playlist)
   {
      romm_notify(MSG_DOWNLOAD_FAILED);
      return;
   }

   if (!(dl = (romm_dl_t*)calloc(1, sizeof(*dl))))
      return;
   notify = MSG_DOWNLOAD_FAILED;

   /* The server address waits in cover_url for the cover to follow it */
   if (!romm_base_url(settings, dl->cover_url, sizeof(dl->cover_url)))
      goto error;
   base_len = strlen(dl->cover_url);

   fill_pathname_join_special(dl->dir, settings->paths.directory_core_assets,
         "RomM", sizeof(dl->dir));
   fill_pathname_join_special(dl->dir, dl->dir, fs_slug, sizeof(dl->dir));
   if (!path_mkdir(dl->dir))
      goto error;
   fill_pathname_join_special(dl->rom_path, dl->dir, fs_name,
         sizeof(dl->rom_path));

   dl->multi = multi;
   if (multi)
   {
      /* Neither the zip nor the unpacking is allowed to land on a
       * file or folder of the user's: both go beside the real names */
      if (   snprintf(dl->zip_path, sizeof(dl->zip_path), "%s.romm-tmp.zip",
               dl->rom_path) >= (int)sizeof(dl->zip_path)
          || snprintf(dl->tmp_dir, sizeof(dl->tmp_dir), "%s.romm-tmp",
               dl->rom_path) >= (int)sizeof(dl->tmp_dir)
          || strlcpy(dl->out_dir, dl->rom_path, sizeof(dl->out_dir))
               >= sizeof(dl->out_dir))
         goto error;
      snprintf(dl->m3u_name, sizeof(dl->m3u_name), "%s.m3u", fs_name);
      strlcpy(dl->label, fs_name, sizeof(dl->label));
   }
   else
   {
      fill_pathname(dl->label, fs_name, "", sizeof(dl->label));
      /* ".hidden" is all extension */
      if (!*dl->label)
         strlcpy(dl->label, fs_name, sizeof(dl->label));
   }
   if (snprintf(dl->part_path, sizeof(dl->part_path), "%s.part",
            multi ? dl->zip_path : dl->rom_path) >= (int)sizeof(dl->part_path))
      goto error;

   /* A folder is only ever made, never replaced, and the zip and the
    * folder it is unpacked into are names this task makes: finding
    * either already there is a download in progress or a leftover to
    * delete by hand. (A .part is not: the transfer truncates it, and
    * the same URL twice is refused by the HTTP task.) */
   if (multi && path_is_valid(dl->out_dir))
   {
      RARCH_ERR("[RomM] \"%s\" is already there.\n", dl->out_dir);
      notify = MSG_ROMM_FOLDER_EXISTS;
      goto refused;
   }
   if (multi && path_is_valid(dl->zip_path))
   {
      RARCH_ERR("[RomM] \"%s\" is in the way.\n", dl->zip_path);
      notify = MSG_ROMM_LEFTOVER_EXISTS;
      goto refused;
   }
   if (multi && path_is_valid(dl->tmp_dir))
   {
      RARCH_ERR("[RomM] \"%s\" is in the way.\n", dl->tmp_dir);
      notify = MSG_ROMM_LEFTOVER_EXISTS;
      goto refused;
   }
   snprintf(dl->db_name, sizeof(dl->db_name), "%s.lpl", system);
   fill_pathname_join_special(dl->playlist_path,
         settings->paths.directory_playlist, dl->db_name,
         sizeof(dl->playlist_path));
   romm_auth_header(settings, dl->header, sizeof(dl->header));

   net_http_urlencode(&encoded, fs_name);
   if (!encoded)
      goto error;
   _len = snprintf(dl->url, sizeof(dl->url), "%s/api/roms/%u/content/",
         dl->cover_url, rom_id);
   if (_len < sizeof(dl->url))
      _len += strlcpy(dl->url + _len, encoded, sizeof(dl->url) - _len);
   free(encoded);
   if (_len >= sizeof(dl->url))
      goto error;

   if (*cover && *settings->paths.directory_thumbnails)
   {
      /* system, slug and cover are spent: reuse them for the names */
      strlcpy(fs_slug, system, sizeof(fs_slug));
      romm_thumbnail_name(fs_slug);
      strlcpy(dl->cover_url + base_len, cover, sizeof(dl->cover_url) - base_len);
      snprintf(cover, sizeof(cover), "%s.png", dl->label);
      romm_thumbnail_name(cover);
      fill_pathname_join_special(dl->dir, settings->paths.directory_thumbnails,
            fs_slug, sizeof(dl->dir));
      fill_pathname_join_special(dl->dir, dl->dir, "Named_Boxarts",
            sizeof(dl->dir));
      fill_pathname_join_special(dl->thumb_path, dl->dir, cover,
            sizeof(dl->thumb_path));
   }

   if (!task_push_http_download_file_with_headers(dl->url,
            dl->part_path,
            *dl->header ? dl->header : NULL, false,
            dl->label, cb_romm_download, dl))
      goto error;
   return;

error:
refused:
   romm_notify(notify);
   romm_dl_free(dl);
}
