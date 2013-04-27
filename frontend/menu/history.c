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
      if (!strcmp(hist->entries[i].path, path) &&
            !strcmp(hist->entries[i].core_path, core_path) &&
            !strcmp(hist->entries[i].core_name, core_name))
      {
         if (i == 0)
            return;

         // Seen it before, bump to top.
         struct rom_history_entry tmp = hist->entries[i];
         memmove(hist->entries + 1, hist->entries,
               (i - 1) * sizeof(struct rom_history_entry));
         hist->entries[0] = tmp;
         return;
      }
   }

   if (hist->size == hist->cap)
      rom_history_free_entry(&hist->entries[hist->cap - 1]);

   memmove(hist->entries + 1, hist->entries,
         (hist->cap - 1) * sizeof(struct rom_history_entry));

   hist->entries[0].path      = strdup(path);
   hist->entries[0].core_path = strdup(core_path);
   hist->entries[0].core_name = strdup(core_name);
}

static void rom_history_write_file(rom_history_t *hist)
{
   char *buf = (char*)malloc(PATH_MAX * 3);
   if (!buf)
      return;

   FILE *file = fopen(hist->conf_path, "w");
   if (!file)
   {
      free(buf);
      return;
   }

   for (size_t i = 0; i < hist->size; i++)
   {
      snprintf(buf, PATH_MAX * 3, "%s;%s;%s\n",
            hist->entries[i].path,
            hist->entries[i].core_path,
            hist->entries[i].core_name);
   }

   fclose(file);
   free(buf);
}

void rom_history_free(rom_history_t *hist)
{
   if (!hist)
      return;

   for (size_t i = 0; i < hist->cap; i++)
      rom_history_free_entry(&hist->entries[i]);
   free(hist->entries);

   if (hist->conf_path)
      rom_history_write_file(hist);
   free(hist->conf_path);

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
      return false;
   
   char *buf = (char*)malloc(PATH_MAX * 3);
   if (!buf)
   {
      fclose(file);
      return false;
   }

   for (hist->size = 0;
         hist->size < hist->cap && fgets(buf, PATH_MAX * 3, file);
         hist->size++)
   {
      struct string_list *list = string_split(buf, ";");
      if (!list)
         break;

      if (list->size != 3)
      {
         string_list_free(list);
         break;
      }

      struct rom_history_entry *entry = &hist->entries[hist->size];
      entry->path = strdup(list->elems[0].data);
      entry->core_path = strdup(list->elems[1].data);
      entry->core_name = strdup(list->elems[2].data);
      string_list_free(list);
   }

   fclose(file);
   free(buf);
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

