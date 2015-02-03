/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include "playlist.h"
#include <compat/posix_string.h>
#include <boolean.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * content_playlist_get_index:
 * @playlist        	   : Playlist handle.
 * @idx                 : Index of playlist entry.
 * @path                : Path of playlist entry.
 * @core_path           : Core path of playlist entry.
 * @core_name           : Core name of playlist entry.
 * 
 * Gets values of playlist index: 
 **/
void content_playlist_get_index(content_playlist_t *playlist,
      size_t idx,
      const char **path, const char **core_path,
      const char **core_name)
{
   if (!playlist)
      return;

   if (path)
      *path      = playlist->entries[idx].path;
   if (core_path)
      *core_path = playlist->entries[idx].core_path;
   if (core_name)
      *core_name = playlist->entries[idx].core_name;
}

void content_playlist_get_index_by_path(content_playlist_t *playlist,
      const char *search_path,
      char **path, char **core_path,
      char **core_name)
{
   size_t i;
   if (!playlist)
      return;

   for (i = 0; i < playlist->size; i++)
   {
      if (strcmp(playlist->entries[i].path, search_path) != 0)
         continue;

      if (path)
         *path      = playlist->entries[i].path;
      if (core_path)
         *core_path = playlist->entries[i].core_path;
      if (core_name)
         *core_name = playlist->entries[i].core_name;
      break;
   }

}

/**
 * content_playlist_free_entry:
 * @entry           	   : Playlist entry handle.
 *
 * Frees playlist entry.
 **/
static void content_playlist_free_entry(content_playlist_entry_t *entry)
{
   if (!entry)
      return;

   if (entry->path)
      free(entry->path);
   entry->path = NULL;
   if (entry->core_path)
      free(entry->core_path);
   entry->core_path = NULL;
   if (entry->core_name)
      free(entry->core_name);
   entry->core_name = NULL;

   memset(entry, 0, sizeof(*entry));
}

/**
 * content_playlist_push:
 * @playlist        	   : Playlist handle.
 * @path                : Path of new playlist entry.
 * @core_path           : Core path of new playlist entry.
 * @core_name           : Core name of new playlist entry.
 *
 * Push entry to top of playlist.
 **/
void content_playlist_push(content_playlist_t *playlist,
      const char *path, const char *core_path,
      const char *core_name)
{
   size_t i;

   if (!playlist)
      return;

   for (i = 0; i < playlist->size; i++)
   {
      content_playlist_entry_t tmp;
      bool equal_path = (!path && !playlist->entries[i].path) ||
         (path && playlist->entries[i].path &&
          !strcmp(path,playlist->entries[i].path));

      /* Core name can have changed while still being the same core.
       * Differentiate based on the core path only. */
      if (!equal_path)
         continue;

      if (strcmp(playlist->entries[i].core_path, core_path))
         continue;

      /* If top entry, we don't want to push a new entry since
       * the top and the entry to be pushed are the same. */
      if (i == 0)
         return;

      /* Seen it before, bump to top. */
      tmp = playlist->entries[i];
      memmove(playlist->entries + 1, playlist->entries,
		      i * sizeof(content_playlist_entry_t));
      playlist->entries[0] = tmp;

      return;
   }

   if (playlist->size == playlist->cap)
   {
      content_playlist_free_entry(&playlist->entries[playlist->cap - 1]);
      playlist->size--;
   }

   memmove(playlist->entries + 1, playlist->entries,
         (playlist->cap - 1) * sizeof(content_playlist_entry_t));

   playlist->entries[0].path      = path ? strdup(path) : NULL;
   playlist->entries[0].core_path = strdup(core_path);
   playlist->entries[0].core_name = strdup(core_name);
   playlist->size++;
}

static void content_playlist_write_file(content_playlist_t *playlist)
{
   size_t i;
   FILE *file = NULL;

   if (!playlist)
      return;

   file = fopen(playlist->conf_path, "w");

   if (!file)
      return;

   for (i = 0; i < playlist->size; i++)
      fprintf(file, "%s\n%s\n%s\n",
            playlist->entries[i].path ? playlist->entries[i].path : "",
            playlist->entries[i].core_path,
            playlist->entries[i].core_name);

   fclose(file);
}

/**
 * content_playlist_free:
 * @playlist        	   : Playlist handle.
 *
 * Frees playlist handle.
 */
void content_playlist_free(content_playlist_t *playlist)
{
   size_t i;

   if (!playlist)
      return;

   if (playlist->conf_path)
      content_playlist_write_file(playlist);
   free(playlist->conf_path);

   for (i = 0; i < playlist->cap; i++)
      content_playlist_free_entry(&playlist->entries[i]);
   free(playlist->entries);

   free(playlist);
}

/**
 * content_playlist_clear:
 * @playlist        	   : Playlist handle.
 *
 * Clears all playlist entries in playlist.
 **/
void content_playlist_clear(content_playlist_t *playlist)
{
   size_t i;
   if (!playlist)
      return;

   for (i = 0; i < playlist->cap; i++)
      content_playlist_free_entry(&playlist->entries[i]);
   playlist->size = 0;
}

/**
 * content_playlist_size:
 * @playlist        	   : Playlist handle.
 *
 * Gets size of playlist.
 * Returns: size of playlist.
 **/
size_t content_playlist_size(content_playlist_t *playlist)
{
   if (!playlist)
      return 0;
   return playlist->size;
}

static bool content_playlist_read_file(
      content_playlist_t *playlist, const char *path)
{
   char buf[3][1024];
   unsigned i;
   content_playlist_entry_t *entry = NULL;
   char *last = NULL;
   FILE *file = fopen(path, "r");

   /* If playlist file does not exist,
    * create an empty playlist instead.
    */
   if (!file)
      return true;

   for (playlist->size = 0; playlist->size < playlist->cap; )
   {
      for (i = 0; i < 3; i++)
      {
         *buf[i] = '\0';

         if (!fgets(buf[i], sizeof(buf[i]), file))
            goto end;

         last = strrchr(buf[i], '\n');
         if (last)
            *last = '\0';
      }

      entry = &playlist->entries[playlist->size];

      if (!*buf[1] || !*buf[2])
         continue;

      if (*buf[0])
         entry->path = strdup(buf[0]);
      entry->core_path = strdup(buf[1]);
      entry->core_name = strdup(buf[2]);
      playlist->size++;
   }

end:
   fclose(file);
   return true;
}

/**
 * content_playlist_init:
 * @path            	   : Path to playlist contents file.
 * @size                : Maximum capacity of playlist size.
 *
 * Creates and initializes a playlist.
 *
 * Returns: handle to new playlist if successful, otherwise NULL
 **/
content_playlist_t *content_playlist_init(const char *path, size_t size)
{
   content_playlist_t *playlist = (content_playlist_t*)
      calloc(1, sizeof(*playlist));
   if (!playlist)
      return NULL;

   playlist->entries = (content_playlist_entry_t*)calloc(size,
         sizeof(*playlist->entries));
   if (!playlist->entries)
      goto error;

   playlist->cap = size;

   content_playlist_read_file(playlist, path);

   playlist->conf_path = strdup(path);
   return playlist;

error:
   content_playlist_free(playlist);
   return NULL;
}
