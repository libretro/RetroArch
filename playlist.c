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

void content_playlist_get_index(content_playlist_t *hist,
      size_t index,
      const char **path, const char **core_path,
      const char **core_name)
{
   if (!hist)
      return;

   *path      = hist->entries[index].path;
   *core_path = hist->entries[index].core_path;
   *core_name = hist->entries[index].core_name;
}

static void content_playlist_free_entry(struct content_playlist_entry *entry)
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

void content_playlist_push(content_playlist_t *hist,
      const char *path, const char *core_path,
      const char *core_name)
{
   size_t i;

   if (!hist)
      return;

   for (i = 0; i < hist->size; i++)
   {
      bool equal_path = (!path && !hist->entries[i].path) ||
         (path && hist->entries[i].path && !strcmp(path, hist->entries[i].path));

      // Core name can have changed while still being the same core.
      // Differentiate based on the core path only.
      if (equal_path && !strcmp(hist->entries[i].core_path, core_path))
      {
         if (i == 0)
            return;

         // Seen it before, bump to top.
         struct content_playlist_entry tmp = hist->entries[i];
         memmove(hist->entries + 1, hist->entries,
               i * sizeof(struct content_playlist_entry));
         hist->entries[0] = tmp;
         return;
      }
   }

   if (hist->size == hist->cap)
   {
      content_playlist_free_entry(&hist->entries[hist->cap - 1]);
      hist->size--;
   }

   memmove(hist->entries + 1, hist->entries,
         (hist->cap - 1) * sizeof(struct content_playlist_entry));

   hist->entries[0].path      = path ? strdup(path) : NULL;
   hist->entries[0].core_path = strdup(core_path);
   hist->entries[0].core_name = strdup(core_name);
   hist->size++;
}

static void content_playlist_write_file(content_playlist_t *hist)
{
   size_t i;
   FILE *file = NULL;

   if (!hist)
      return;

   file = fopen(hist->conf_path, "w");

   if (!file)
   {
      RARCH_ERR("Couldn't write to content playlist file: %s.\n", hist->conf_path);
      return;
   }

   for (i = 0; i < hist->size; i++)
   {
      fprintf(file, "%s\n%s\n%s\n",
            hist->entries[i].path ? hist->entries[i].path : "",
            hist->entries[i].core_path,
            hist->entries[i].core_name);
   }

   fclose(file);
}

void content_playlist_free(content_playlist_t *hist)
{
   size_t i;
   if (!hist)
      return;

   if (hist->conf_path)
      content_playlist_write_file(hist);
   free(hist->conf_path);

   for (i = 0; i < hist->cap; i++)
      content_playlist_free_entry(&hist->entries[i]);
   free(hist->entries);

   free(hist);
}

void content_playlist_clear(content_playlist_t *hist)
{
   size_t i;
   if (!hist)
      return;

   for (i = 0; i < hist->cap; i++)
      content_playlist_free_entry(&hist->entries[i]);
   hist->size = 0;
}

size_t content_playlist_size(content_playlist_t *hist)
{
   if (hist)
      return hist->size;
   return 0;
}

static bool content_playlist_read_file(content_playlist_t *hist, const char *path)
{
   char buf[3][PATH_MAX];
   unsigned i;
   struct content_playlist_entry *entry = NULL;
   char *last = NULL;
   FILE *file = fopen(path, "r");

   if (!file || !hist)
   {
      RARCH_ERR("Couldn't read content playlist file: %s.\n", path);
      return true;
   }

   for (hist->size = 0; hist->size < hist->cap; )
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

      entry = &hist->entries[hist->size];

      if (!*buf[1] || !*buf[2])
         continue;

      if (*buf[0])
         entry->path = strdup(buf[0]);
      entry->core_path = strdup(buf[1]);
      entry->core_name = strdup(buf[2]);
      hist->size++;
   }

end:
   fclose(file);
   return true;
}

content_playlist_t *content_playlist_init(const char *path, size_t size)
{
   RARCH_LOG("Opening playlist: %s.\n", path);
   content_playlist_t *hist = (content_playlist_t*)calloc(1, sizeof(*hist));
   if (!hist)
   {
       RARCH_ERR("Cannot initialize content playlist.\n");
      return NULL;
   }

   hist->entries = (struct content_playlist_entry*)calloc(size, sizeof(*hist->entries));
   if (!hist->entries)
      goto error;

   hist->cap = size;

   if (!content_playlist_read_file(hist, path))
      goto error;

   hist->conf_path = strdup(path);
   return hist;

error:
   content_playlist_free(hist);
   return NULL;
}

const char* content_playlist_get_path(content_playlist_t *hist, unsigned index)
{
   const char *path, *core_path, *core_name = NULL;
   if (!hist)
      return "";

   content_playlist_get_index(hist, index, &path, &core_path, &core_name);

   if (path)
      return path;
   return "";
}

const char *content_playlist_get_core_path(content_playlist_t *hist, unsigned index)
{
   const char *path, *core_path, *core_name = NULL;
   if (!hist)
      return "";

   content_playlist_get_index(hist, index, &path, &core_path, &core_name);
    
   if (core_path)
      return core_path;
   return "";
}

const char *content_playlist_get_core_name(content_playlist_t *hist, unsigned index)
{
   const char *path, *core_path, *core_name = NULL;
   if (!hist)
      return "";

   content_playlist_get_index(hist, index, &path, &core_path, &core_name);

   if (core_name)
      return core_name;
   return "";
}
