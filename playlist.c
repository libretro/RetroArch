/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2013-2014 - Jason Fetters
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
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <compat/posix_string.h>
#include <string/stdstring.h>
#include <streams/file_stream.h>
#include <retro_miscellaneous.h>
#include <file/file_path.h>

#include "playlist.h"
#include "verbosity.h"

#ifndef PLAYLIST_ENTRIES
#define PLAYLIST_ENTRIES 6
#endif

struct playlist_entry
{
   char *path;
   char *label;
   char *core_path;
   char *core_name;
   char *db_name;
   char *crc32;
};

struct content_playlist
{
   struct playlist_entry *entries;
   size_t size;
   size_t cap;

   char *conf_path;
};

/**
 * playlist_get_index:
 * @playlist            : Playlist handle.
 * @idx                 : Index of playlist entry.
 * @path                : Path of playlist entry.
 * @core_path           : Core path of playlist entry.
 * @core_name           : Core name of playlist entry.
 * 
 * Gets values of playlist index: 
 **/
void playlist_get_index(playlist_t *playlist,
      size_t idx,
      const char **path, const char **label,
      const char **core_path, const char **core_name,
      const char **crc32,
      const char **db_name)
{
   if (!playlist)
      return;

   if (path)
      *path      = playlist->entries[idx].path;
   if (label)
      *label     = playlist->entries[idx].label;
   if (core_path)
      *core_path = playlist->entries[idx].core_path;
   if (core_name)
      *core_name = playlist->entries[idx].core_name;
   if (db_name)
      *db_name   = playlist->entries[idx].db_name;
   if (crc32)
      *crc32     = playlist->entries[idx].crc32;
}

void playlist_get_index_by_path(playlist_t *playlist,
      const char *search_path,
      char **path, char **label,
      char **core_path, char **core_name,
      char **crc32,
      char **db_name)
{
   size_t i;
   if (!playlist)
      return;

   for (i = 0; i < playlist->size; i++)
   {
      if (!string_is_equal(playlist->entries[i].path, search_path))
         continue;

      if (path)
         *path      = playlist->entries[i].path;
      if (label)
         *label     = playlist->entries[i].label;
      if (core_path)
         *core_path = playlist->entries[i].core_path;
      if (core_name)
         *core_name = playlist->entries[i].core_name;
      if (db_name)
         *db_name   = playlist->entries[i].db_name;
      if (crc32)
         *crc32     = playlist->entries[i].crc32;
      break;
   }
}

bool playlist_entry_exists(playlist_t *playlist,
      const char *path,
      const char *crc32)
{
   size_t i;
   if (!playlist)
      return false;

   for (i = 0; i < playlist->size; i++)
      if (string_is_equal(playlist->entries[i].path, path))
         return true;

   return false;
}

/**
 * playlist_free_entry:
 * @entry               : Playlist entry handle.
 *
 * Frees playlist entry.
 **/
static void playlist_free_entry(playlist_entry_t *entry)
{
   if (!entry)
      return;

   if (entry->path)
      free(entry->path);
   entry->path = NULL;

   if (entry->label)
      free(entry->label);
   entry->label = NULL;

   if (entry->core_path)
      free(entry->core_path);
   entry->core_path = NULL;

   if (entry->core_name)
      free(entry->core_name);
   entry->core_name = NULL;

   if (entry->db_name)
      free(entry->db_name);
   entry->core_name = NULL;

   if (entry->crc32)
      free(entry->crc32);
   entry->crc32 = NULL;

   memset(entry, 0, sizeof(*entry));
}

void playlist_update(playlist_t *playlist, size_t idx,
      const char *path, const char *label,
      const char *core_path, const char *core_name,
      const char *crc32,
      const char *db_name)
{
   playlist_entry_t *entry = NULL;
   if (!playlist)
      return;
   if (idx > playlist->size)
      return;

   entry            = &playlist->entries[idx];

   if (path && (path != entry->path)) {
      free(entry->path);
      entry->path = strdup(path);
   }
   if (label && (label != entry->label)) {
      free(entry->label);
      entry->label = strdup(label);
   }
   if (core_path && (core_path != entry->core_path)) {
      free(entry->core_path);
      entry->core_path = strdup(core_path);
   }
   if (core_name && (core_name != entry->core_name)) {
      free(entry->core_name);
      entry->core_name = strdup(core_name);
   }
   if (db_name && (db_name != entry->db_name)) {
      free(entry->db_name);
      entry->db_name = strdup(db_name);
   }
   if (crc32 && (crc32 != entry->crc32)) {
      free(entry->crc32);
      entry->crc32 = strdup(crc32);
   }
}

/**
 * playlist_push:
 * @playlist        	   : Playlist handle.
 * @path                : Path of new playlist entry.
 * @core_path           : Core path of new playlist entry.
 * @core_name           : Core name of new playlist entry.
 *
 * Push entry to top of playlist.
 **/
bool playlist_push(playlist_t *playlist,
      const char *path, const char *label,
      const char *core_path, const char *core_name,
      const char *crc32,
      const char *db_name)
{
   size_t i;

   if (!playlist)
      return false;

   if (string_is_empty(core_path) || string_is_empty(core_name))
   {
      if (string_is_empty(core_name) && !string_is_empty(core_path))
      {
         static char base_path[PATH_MAX_LENGTH] = {0};
         fill_pathname_base_noext(base_path, core_path, sizeof(base_path));
         core_name = base_path;
         RARCH_LOG("core_name is now: %s\n", core_name);
      }

      RARCH_LOG("core_name: %s.\n", string_is_empty(core_name) ? "N/A" : core_name);
      RARCH_LOG("core_path: %s.\n", string_is_empty(core_path) ? "N/A" : core_path);

      if (string_is_empty(core_path) || string_is_empty(core_name))
      {
         RARCH_ERR("cannot push NULL or empty core name into the playlist.\n");
         return false;
      }
   }

   if (string_is_empty(path))
      path = NULL;

   for (i = 0; i < playlist->size; i++)
   {
      playlist_entry_t tmp;
      bool equal_path = (!path && !playlist->entries[i].path) ||
         (path && playlist->entries[i].path &&
          string_is_equal(path,playlist->entries[i].path));

      /* Core name can have changed while still being the same core.
       * Differentiate based on the core path only. */
      if (!equal_path)
         continue;

      if (!string_is_equal(playlist->entries[i].core_path, core_path))
         continue;

      /* If top entry, we don't want to push a new entry since
       * the top and the entry to be pushed are the same. */
      if (i == 0)
         return false;

      /* Seen it before, bump to top. */
      tmp = playlist->entries[i];
      memmove(playlist->entries + 1, playlist->entries,
            i * sizeof(playlist_entry_t));
      playlist->entries[0] = tmp;

      return true;
   }

   if (playlist->size == playlist->cap)
   {
      playlist_free_entry(&playlist->entries[playlist->cap - 1]);
      playlist->size--;
   }

   memmove(playlist->entries + 1, playlist->entries,
         (playlist->cap - 1) * sizeof(playlist_entry_t));

   playlist->entries[0].path      = NULL;
   playlist->entries[0].label     = NULL;
   playlist->entries[0].core_path = NULL;
   playlist->entries[0].core_name = NULL;
   playlist->entries[0].db_name   = NULL;
   playlist->entries[0].crc32     = NULL;
   if (!string_is_empty(path))
      playlist->entries[0].path   = strdup(path);
   if (!string_is_empty(label))
      playlist->entries[0].label  = strdup(label);
   if (!string_is_empty(core_path))
      playlist->entries[0].core_path = strdup(core_path);
   if (!string_is_empty(core_name))
      playlist->entries[0].core_name = strdup(core_name);
   if (!string_is_empty(db_name))
      playlist->entries[0].db_name   = strdup(db_name);
   if (!string_is_empty(crc32))
      playlist->entries[0].crc32     = strdup(crc32);

   playlist->size++;

   return true;
}

void playlist_write_file(playlist_t *playlist)
{
   size_t i;
   FILE *file = NULL;

   if (!playlist)
      return;

   file = fopen(playlist->conf_path, "w");

   RARCH_LOG("Trying to write to playlist file: %s\n", playlist->conf_path);

   if (!file)
   {
      RARCH_ERR("Failed to write to playlist file: %s\n", playlist->conf_path);
      return;
   }

   for (i = 0; i < playlist->size; i++)
      fprintf(file, "%s\n%s\n%s\n%s\n%s\n%s\n",
            playlist->entries[i].path    ? playlist->entries[i].path    : "",
            playlist->entries[i].label   ? playlist->entries[i].label   : "",
            playlist->entries[i].core_path,
            playlist->entries[i].core_name,
            playlist->entries[i].crc32   ? playlist->entries[i].crc32   : "",
            playlist->entries[i].db_name ? playlist->entries[i].db_name : ""
            );

   fclose(file);
}

/**
 * playlist_free:
 * @playlist            : Playlist handle.
 *
 * Frees playlist handle.
 */
void playlist_free(playlist_t *playlist)
{
   size_t i;

   if (!playlist)
      return;

   if (playlist->conf_path)
      free(playlist->conf_path);

   playlist->conf_path = NULL;

   for (i = 0; i < playlist->cap; i++)
      playlist_free_entry(&playlist->entries[i]);

   free(playlist->entries);
   playlist->entries = NULL;

   free(playlist);
}

/**
 * playlist_clear:
 * @playlist        	   : Playlist handle.
 *
 * Clears all playlist entries in playlist.
 **/
void playlist_clear(playlist_t *playlist)
{
   size_t i;
   if (!playlist)
      return;

   for (i = 0; i < playlist->cap; i++)
      playlist_free_entry(&playlist->entries[i]);
   playlist->size = 0;
}

/**
 * playlist_size:
 * @playlist        	   : Playlist handle.
 *
 * Gets size of playlist.
 * Returns: size of playlist.
 **/
size_t playlist_size(playlist_t *playlist)
{
   if (!playlist)
      return 0;
   return playlist->size;
}

const char *playlist_entry_get_label(
      const playlist_entry_t *entry)
{
   if (!entry)
      return NULL;
   return entry->label;
}

static bool playlist_read_file(
      playlist_t *playlist, const char *path)
{
   unsigned i;
   char buf[PLAYLIST_ENTRIES][1024] = {{0}};
   playlist_entry_t *entry  = NULL;
   char *last                       = NULL;
   RFILE *file                      = filestream_open(path, RFILE_MODE_READ_TEXT, -1);

   /* If playlist file does not exist,
    * create an empty playlist instead.
    */
   if (!file)
      return true;

   for (playlist->size = 0; playlist->size < playlist->cap; )
   {
      for (i = 0; i < PLAYLIST_ENTRIES; i++)
      {
         *buf[i] = '\0';

         if (!filestream_gets(file, buf[i], sizeof(buf[i])))
            goto end;

         last = strrchr(buf[i], '\n');
         if (last)
            *last = '\0';
      }

      entry = &playlist->entries[playlist->size];

      if (!*buf[2] || !*buf[3])
         continue;

      if (*buf[0])
         entry->path      = strdup(buf[0]);
      if (*buf[1])
         entry->label     = strdup(buf[1]);
      entry->core_path    = strdup(buf[2]);
      entry->core_name    = strdup(buf[3]);
      if (*buf[4])
         entry->crc32     = strdup(buf[4]);
      if (*buf[5])
         entry->db_name   = strdup(buf[5]);
      playlist->size++;
   }

end:
   filestream_close(file);
   return true;
}

/**
 * playlist_init:
 * @path            	   : Path to playlist contents file.
 * @size                : Maximum capacity of playlist size.
 *
 * Creates and initializes a playlist.
 *
 * Returns: handle to new playlist if successful, otherwise NULL
 **/
playlist_t *playlist_init(const char *path, size_t size)
{
   playlist_t *playlist = (playlist_t*)
      calloc(1, sizeof(*playlist));
   if (!playlist)
      return NULL;

   playlist->entries = (playlist_entry_t*)calloc(size,
         sizeof(*playlist->entries));
   if (!playlist->entries)
      goto error;

   playlist->cap = size;

   playlist_read_file(playlist, path);

   playlist->conf_path = strdup(path);
   return playlist;

error:
   playlist_free(playlist);
   return NULL;
}

void playlist_qsort(playlist_t *playlist,
      playlist_sort_fun_t *fn)
{
   qsort(playlist->entries, playlist->size,
         sizeof(playlist_entry_t),
         (int (*)(const void *, const void *))fn);
}
