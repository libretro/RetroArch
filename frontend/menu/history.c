/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

#include "history.h"
#include "../../msvc/msvc_compat.h"
#include "../../compat/posix_string.h"
#include "../../boolean.h"
#include "../../general.h"
#include "../../file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct rom_history_entry
{
   char *path;
   char *core_path;
   char *core_name;
};

struct rom_history
{
   struct rom_history_entry *entries;
   size_t size;
   size_t cap;

   char *conf_path;
};

void rom_history_get_index(rom_history_t *hist,
      size_t index,
      const char **path, const char **core_path,
      const char **core_name)
{
   *path      = hist->entries[index].path;
   *core_path = hist->entries[index].core_path;
   *core_name = hist->entries[index].core_name;
}

static void rom_history_free_entry(struct rom_history_entry *entry)
{
   free(entry->path);
   free(entry->core_path);
   free(entry->core_name);
   memset(entry, 0, sizeof(*entry));
}

void rom_history_push(rom_history_t *hist,
      const char *path, const char *core_path,
      const char *core_name)
{
   for (size_t i = 0; i < hist->size; i++)
   {
      bool equal_path = (!path && !hist->entries[i].path) ||
         (path && hist->entries[i].path && !strcmp(path, hist->entries[i].path));

      if (equal_path &&
            !strcmp(hist->entries[i].core_path, core_path) &&
            !strcmp(hist->entries[i].core_name, core_name))
      {
         if (i == 0)
            return;

         // Seen it before, bump to top.
         struct rom_history_entry tmp = hist->entries[i];
         memmove(hist->entries + 1, hist->entries,
               i * sizeof(struct rom_history_entry));
         hist->entries[0] = tmp;
         return;
      }
   }

   if (hist->size == hist->cap)
   {
      rom_history_free_entry(&hist->entries[hist->cap - 1]);
      hist->size--;
   }

   memmove(hist->entries + 1, hist->entries,
         (hist->cap - 1) * sizeof(struct rom_history_entry));

   hist->entries[0].path      = path ? strdup(path) : NULL;
   hist->entries[0].core_path = strdup(core_path);
   hist->entries[0].core_name = strdup(core_name);
   hist->size++;
}

static void rom_history_write_file(rom_history_t *hist)
{
   FILE *file = fopen(hist->conf_path, "w");
   if (!file)
      return;

   for (size_t i = 0; i < hist->size; i++)
   {
      fprintf(file, "%s\n%s\n%s\n",
            hist->entries[i].path ? hist->entries[i].path : "",
            hist->entries[i].core_path,
            hist->entries[i].core_name);
   }

   fclose(file);
}

void rom_history_free(rom_history_t *hist)
{
   if (!hist)
      return;

   if (hist->conf_path)
      rom_history_write_file(hist);
   free(hist->conf_path);

   for (size_t i = 0; i < hist->cap; i++)
      rom_history_free_entry(&hist->entries[i]);
   free(hist->entries);

   free(hist);
}

size_t rom_history_size(rom_history_t *hist)
{
   return hist->size;
}

static bool rom_history_read_file(rom_history_t *hist, const char *path)
{
   FILE *file = fopen(path, "r");
   if (!file)
      return true;
   
   char buf[3][PATH_MAX];
   struct rom_history_entry *entry = NULL;
   char *last = NULL;

   for (hist->size = 0; hist->size < hist->cap; hist->size++)
   {
      for (unsigned i = 0; i < 3; i++)
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
         goto end;


      if (*buf[0])
         entry->path = strdup(buf[0]);
      entry->core_path = strdup(buf[1]);
      entry->core_name = strdup(buf[2]);
   }

end:
   fclose(file);
   return true;
}

rom_history_t *rom_history_init(const char *path, size_t size)
{
   rom_history_t *hist = (rom_history_t*)calloc(1, sizeof(*hist));
   if (!hist)
      return NULL;

   hist->entries = (struct rom_history_entry*)calloc(size, sizeof(*hist->entries));
   if (!hist->entries)
      goto error;

   hist->cap = size;

   if (!rom_history_read_file(hist, path))
      goto error;

   hist->conf_path = strdup(path);
   return hist;

error:
   rom_history_free(hist);
   return NULL;
}

