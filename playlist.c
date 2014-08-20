/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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
#include "msvc/msvc_compat.h"
#include "compat/posix_string.h"
#include "boolean.h"
#include "general.h"
#include "file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct content_playlist_entry
{
   char *path;
   char *core_path;
   char *core_name;
};

struct content_playlist
{
   struct content_playlist_entry *entries;
   size_t size;
   size_t cap;

   char *conf_path;
};

void content_playlist_get_index(content_playlist_t *playlist,
      size_t index,
      const char **path, const char **core_path,
      const char **core_name)
{
   if (!playlist)
      return;

   *path      = playlist->entries[index].path;
   *core_path = playlist->entries[index].core_path;
   *core_name = playlist->entries[index].core_name;
}

static void content_playlist_free_entry(
      struct content_playlist_entry *entry)
{
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

void content_playlist_push(content_playlist_t *playlist,
      const char *path, const char *core_path,
      const char *core_name)
{
   size_t i;

   if (!playlist)
      return;

   for (i = 0; i < playlist->size; i++)
   {
      bool equal_path = (!path && !playlist->entries[i].path) ||
         (path && playlist->entries[i].path &&
          !strcmp(path,playlist->entries[i].path));

      /* Core name can have changed while still being the same core.
       * Differentiate based on the core path only. */
      if (equal_path && !strcmp(playlist->entries[i].core_path, core_path))
      {
         if (i == 0)
            return;

         /* Seen it before, bump to top. */
         struct content_playlist_entry tmp = playlist->entries[i];
         memmove(playlist->entries + 1, playlist->entries,
               i * sizeof(struct content_playlist_entry));
         playlist->entries[0] = tmp;
         return;
      }
   }

   if (playlist->size == playlist->cap)
   {
      content_playlist_free_entry(&playlist->entries[playlist->cap - 1]);
      playlist->size--;
   }

   memmove(playlist->entries + 1, playlist->entries,
         (playlist->cap - 1) * sizeof(struct content_playlist_entry));

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
   {
      RARCH_ERR("Couldn't write to content playlist file: %s.\n",
            playlist->conf_path);
      return;
   }

   for (i = 0; i < playlist->size; i++)
   {
      fprintf(file, "%s\n%s\n%s\n",
            playlist->entries[i].path ? playlist->entries[i].path : "",
            playlist->entries[i].core_path,
            playlist->entries[i].core_name);
   }

   fclose(file);
}

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

void content_playlist_clear(content_playlist_t *playlist)
{
   size_t i;
   if (!playlist)
      return;

   for (i = 0; i < playlist->cap; i++)
      content_playlist_free_entry(&playlist->entries[i]);
   playlist->size = 0;
}

size_t content_playlist_size(content_playlist_t *playlist)
{
   if (playlist)
      return playlist->size;
   return 0;
}

static bool content_playlist_read_file(
      content_playlist_t *playlist, const char *path)
{
   char buf[3][PATH_MAX];
   unsigned i;
   struct content_playlist_entry *entry = NULL;
   char *last = NULL;
   FILE *file = fopen(path, "r");

   if (!file || !playlist)
   {
      RARCH_ERR("Couldn't read content playlist file: %s.\n", path);
      return true;
   }

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

content_playlist_t *content_playlist_init(const char *path, size_t size)
{
   RARCH_LOG("Opening playlist: %s.\n", path);
   content_playlist_t *playlist = (content_playlist_t*)
      calloc(1, sizeof(*playlist));
   if (!playlist)
   {
       RARCH_ERR("Cannot initialize content playlist.\n");
      return NULL;
   }

   playlist->entries = (struct content_playlist_entry*)calloc(size,
         sizeof(*playlist->entries));
   if (!playlist->entries)
      goto error;

   playlist->cap = size;

   if (!content_playlist_read_file(playlist, path))
      goto error;

   playlist->conf_path = strdup(path);
   return playlist;

error:
   content_playlist_free(playlist);
   return NULL;
}
