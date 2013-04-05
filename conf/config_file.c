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


#include "config_file.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include "../compat/strl.h"
#include "../compat/posix_string.h"
#include "../msvc/msvc_compat.h"
#include "../file.h"

#if !defined(_WIN32) && !defined(__CELLOS_LV2__) && !defined(_XBOX)
#include <sys/param.h> // PATH_MAX
#elif defined(_WIN32) && !defined(_XBOX)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(_XBOX)
#include <xtl.h>
#endif

#ifndef PATH_MAX
#ifdef PATH_MAX
#define PATH_MAX PATH_MAX
#else
#define PATH_MAX 512
#endif
#endif

#define MAX_INCLUDE_DEPTH 16

struct config_entry_list
{
   bool readonly; // If we got this from an #include, do not allow write.
   char *key;
   char *value;
   struct config_entry_list *next;
};

struct include_list
{
   char *path;
   struct include_list *next;
};

struct config_file
{
   char *path;
   struct config_entry_list *entries;
   struct config_entry_list *tail;
   unsigned include_depth;

   struct include_list *includes;
};

static config_file_t *config_file_new_internal(const char *path, unsigned depth);

static char *getaline(FILE *file)
{
   char *newline = (char*)malloc(9);
   size_t cur_size = 8;
   size_t index = 0;

   int in = getc(file);
   while (in != EOF && in != '\n')
   {
      if (index == cur_size)
      {
         cur_size *= 2;
         newline = (char*)realloc(newline, cur_size + 1);
      }

      newline[index++] = in;
      in = getc(file);
   }
   newline[index] = '\0';
   return newline; 
}

static char *extract_value(char *line, bool is_value)
{
   if (is_value)
   {
      while (isspace(*line))
         line++;

      // If we don't have an equal sign here, we've got an invalid string...
      if (*line != '=')
         return NULL;

      line++;
   }

   while (isspace(*line))
      line++;

   char *save;
   char *tok;

   // We have a full string. Read until next ".
   if (*line == '"')
   {
      line++;
      tok = strtok_r(line, "\"", &save);
      if (!tok)
         return NULL;
      return strdup(tok);
   }
   else if (*line == '\0') // Nothing :(
      return NULL;
   else // We don't have that... Read till next space.
   {
      tok = strtok_r(line, " \n\t\f\r\v", &save);
      if (tok)
         return strdup(tok);
      else
         return NULL;
   }
}

static void set_list_readonly(struct config_entry_list *list)
{
   while (list)
   {
      list->readonly = true;
      list = list->next;
   }
}

// Move semantics? :)
static void add_child_list(config_file_t *parent, config_file_t *child)
{
   if (parent->entries)
   {
      struct config_entry_list *head = parent->entries;
      while (head->next)
         head = head->next;

      set_list_readonly(child->entries);
      head->next = child->entries;
   }
   else
   {
      set_list_readonly(child->entries);
      parent->entries = child->entries;
   }

   child->entries = NULL;

   // Rebase tail.
   if (parent->entries)
   {
      struct config_entry_list *head = parent->entries;
      while (head->next)
         head = head->next;
      parent->tail = head;
   }
   else
      parent->tail = NULL;
}

static void add_include_list(config_file_t *conf, const char *path)
{
   struct include_list *head = conf->includes;
   struct include_list *node = (struct include_list*)calloc(1, sizeof(*node));
   node->path = strdup(path);

   if (head)
   {
      while (head->next)
         head = head->next;

      head->next = node;
   }
   else
      conf->includes = node;
}

static void add_sub_conf(config_file_t *conf, char *line)
{
   char *path = extract_value(line, false);
   if (!path)
      return;

   add_include_list(conf, path);
   char real_path[PATH_MAX];

#ifdef _WIN32
   fill_pathname_resolve_relative(real_path, conf->path, path, sizeof(real_path));
#else
#ifndef __CELLOS_LV2__
   if (*path == '~')
   {
      const char *home = getenv("HOME");
      strlcpy(real_path, home ? home : "", sizeof(real_path));
      strlcat(real_path, path + 1, sizeof(real_path));
   }
   else
#endif
      fill_pathname_resolve_relative(real_path, conf->path, path, sizeof(real_path));
#endif

   config_file_t *sub_conf = config_file_new_internal(real_path, conf->include_depth + 1);
   if (!sub_conf)
   {
      free(path);
      return;
   }

   // Pilfer internal list. :D
   add_child_list(conf, sub_conf);
   config_file_free(sub_conf);
   free(path);
}

static bool parse_line(config_file_t *conf, struct config_entry_list *list, char *line)
{
   // Remove everything after comment.
   char *comment = strchr(line, '#');
   if (comment)
      *comment = '\0';

   // Starting line with # and include includes config files. :)
   if ((comment == line) && (conf->include_depth < MAX_INCLUDE_DEPTH))
   {
      comment++;
      if (strstr(comment, "include ") == comment)
      {
         add_sub_conf(conf, comment + strlen("include "));
         return false;
      }
   }
   else if (conf->include_depth >= MAX_INCLUDE_DEPTH)
      fprintf(stderr, "!!! #include depth exceeded for config. Might be a cycle.\n");

   // Skips to first character.
   while (isspace(*line))
      line++;

   char *key = (char*)malloc(9);
   size_t cur_size = 8;
   size_t index = 0;

   while (isgraph(*line))
   {
      if (index == cur_size)
      {
         cur_size *= 2;
         key = (char*)realloc(key, cur_size + 1);
      }

      key[index++] = *line++;
   }
   key[index] = '\0';
   list->key = key;

   list->value = extract_value(line, true);
   if (!list->value)
   {
      list->key = NULL;
      free(key);
      return false;
   }

   return true;
}

bool config_append_file(config_file_t *conf, const char *path)
{
   config_file_t *new_conf = config_file_new(path);
   if (!new_conf)
      return false;

   if (new_conf->tail)
   {
      new_conf->tail->next = conf->entries;
      conf->entries        = new_conf->entries; // Pilfer.
      new_conf->entries    = NULL;
   }

   config_file_free(new_conf);
   return true;
}

static config_file_t *config_file_new_internal(const char *path, unsigned depth)
{
   struct config_file *conf = (struct config_file*)calloc(1, sizeof(*conf));
   if (!conf)
      return NULL;

   if (!path)
      return conf;

   conf->path = strdup(path);
   if (!conf->path)
   {
      free(conf);
      return NULL;
   }

   conf->include_depth = depth;
   FILE *file = fopen(path, "r");

   if (!file)
   {
      free(conf->path);
      free(conf);
      return NULL;
   }

   while (!feof(file))
   {
      struct config_entry_list *list = (struct config_entry_list*)calloc(1, sizeof(*list));
      char *line = getaline(file);

      if (line)
      {
         if (parse_line(conf, list, line))
         {
            if (conf->entries)
            {
               conf->tail->next = list;
               conf->tail = list;
            }
            else
            {
               conf->entries = list;
               conf->tail = list;
            }
         }

         free(line);
      }

      if (list != conf->tail)
         free(list);
   }
   fclose(file);

   return conf;
}

config_file_t *config_file_new(const char *path)
{
   return config_file_new_internal(path, 0);
}

void config_file_free(config_file_t *conf)
{
   if (!conf)
      return;

   struct config_entry_list *tmp = conf->entries;
   while (tmp)
   {
      free(tmp->key);
      free(tmp->value);
      struct config_entry_list *hold = tmp;
      tmp = tmp->next;
      free(hold);
   }

   struct include_list *inc_tmp = conf->includes;
   while (inc_tmp)
   {
      free(inc_tmp->path);
      struct include_list *hold = inc_tmp;
      inc_tmp = inc_tmp->next;
      free(hold);
   }

   free(conf->path);
   free(conf);
}

bool config_get_double(config_file_t *conf, const char *key, double *in)
{
   struct config_entry_list *list = conf->entries;

   while (list)
   {
      if (strcmp(key, list->key) == 0)
      {
         *in = strtod(list->value, NULL);
         return true;
      }
      list = list->next;
   }
   return false;
}

bool config_get_float(config_file_t *conf, const char *key, float *in)
{
   struct config_entry_list *list = conf->entries;

   while (list)
   {
      if (strcmp(key, list->key) == 0)
      {
         // strtof() is C99/POSIX. Just use the more portable kind.
         *in = (float)strtod(list->value, NULL);
         return true;
      }
      list = list->next;
   }
   return false;
}

bool config_get_int(config_file_t *conf, const char *key, int *in)
{
   struct config_entry_list *list = conf->entries;

   while (list)
   {
      if (strcmp(key, list->key) == 0)
      {
         errno = 0;
         int val = strtol(list->value, NULL, 0);
         if (errno == 0)
         {
            *in = val;
            return true;
         }
         else
            return false;
      }
      list = list->next;
   }
   return false;
}

bool config_get_uint64(config_file_t *conf, const char *key, uint64_t *in)
{
   struct config_entry_list *list = conf->entries;

   while (list)
   {
      if (strcmp(key, list->key) == 0)
      {
         errno = 0;
         uint64_t val = strtoull(list->value, NULL, 0);
         if (errno == 0)
         {
            *in = val;
            return true;
         }
         else
            return false;
      }
      list = list->next;
   }
   return false;
}

bool config_get_uint(config_file_t *conf, const char *key, unsigned *in)
{
   struct config_entry_list *list = conf->entries;

   while (list != NULL)
   {
      if (strcmp(key, list->key) == 0)
      {
         errno = 0;
         unsigned val = strtoul(list->value, NULL, 0);
         if (errno == 0)
         {
            *in = val;
            return true;
         }
         else
            return false;
      }
      list = list->next;
   }

   return false;
}

bool config_get_hex(config_file_t *conf, const char *key, unsigned *in)
{
   struct config_entry_list *list = conf->entries;

   while (list)
   {
      if (strcmp(key, list->key) == 0)
      {
         errno = 0;
         unsigned val = strtoul(list->value, NULL, 16);
         if (errno == 0)
         {
            *in = val;
            return true;
         }
         else
            return false;
      }
      list = list->next;
   }
   return false;
}

bool config_get_char(config_file_t *conf, const char *key, char *in)
{
   struct config_entry_list *list = conf->entries;

   while (list)
   {
      if (strcmp(key, list->key) == 0)
      {
         if (list->value[0] && list->value[1])
            return false;
         *in = *list->value;
         return true;
      }
      list = list->next;
   }
   return false;
}

bool config_get_string(config_file_t *conf, const char *key, char **str)
{
   struct config_entry_list *list = conf->entries;

   while (list)
   {
      if (strcmp(key, list->key) == 0)
      {
         *str = strdup(list->value);
         return true;
      }
      list = list->next;
   }
   return false;
}

bool config_get_array(config_file_t *conf, const char *key, char *buf, size_t size)
{
   struct config_entry_list *list = conf->entries;

   while (list)
   {
      if (strcmp(key, list->key) == 0)
         return strlcpy(buf, list->value, size) < size;
      list = list->next;
   }
   return false;
}

bool config_get_path(config_file_t *conf, const char *key, char *buf, size_t size)
{
#if defined(RARCH_CONSOLE)
   return config_get_array(conf, key, buf, size);
#else
   struct config_entry_list *list = conf->entries;

   while (list)
   {
      if (strcmp(key, list->key) == 0)
      {
         const char *value = list->value;

         if (*value == '~')
         {
            const char *home = getenv("HOME");
            if (home)
            {
               size_t src_size = strlcpy(buf, home, size);
               if (src_size >= size)
                  return false;

               buf  += src_size;
               size -= src_size;
               value++;
            }
         }
         else if ((*value == ':') &&
#ifdef _WIN32
               ((value[1] == '/') || (value[1] == '\\')))
#else
               (value[1] == '/'))
#endif
         {
            char application_dir[PATH_MAX];
            fill_pathname_application_path(application_dir, sizeof(application_dir));

            RARCH_LOG("[Config]: Querying application path: %s.\n", application_dir);
            path_basedir(application_dir);

            size_t src_size = strlcpy(buf, application_dir, size);
            if (src_size >= size)
               return false;

            buf  += src_size;
            size -= src_size;
            value += 2;
         }

         return strlcpy(buf, value, size) < size;
      }
      list = list->next;
   }
   return false;
#endif
}

bool config_get_bool(config_file_t *conf, const char *key, bool *in)
{
   struct config_entry_list *list = conf->entries;

   while (list)
   {
      if (strcmp(key, list->key) == 0)
      {
         if (strcasecmp(list->value, "true") == 0)
            *in = true;
         else if (strcasecmp(list->value, "1") == 0)
            *in = true;
         else if (strcasecmp(list->value, "false") == 0)
            *in = false;
         else if (strcasecmp(list->value, "0") == 0)
            *in = false;
         else
            return false;

         return true;
      }
      list = list->next;
   }
   return false;
}

void config_set_string(config_file_t *conf, const char *key, const char *val)
{
   struct config_entry_list *list = conf->entries;
   struct config_entry_list *last = list;
   while (list)
   {
      if (!list->readonly && (strcmp(key, list->key) == 0))
      {
         free(list->value);
         list->value = strdup(val);
         return;
      }

      last = list;
      list = list->next;
   }

   struct config_entry_list *elem = (struct config_entry_list*)calloc(1, sizeof(*elem));
   elem->key = strdup(key);
   elem->value = strdup(val);

   if (last)
      last->next = elem;
   else
      conf->entries = elem;
}

void config_set_double(config_file_t *conf, const char *key, double val)
{
   char buf[128];
#ifdef __cplusplus
   snprintf(buf, sizeof(buf), "%f", (float)val);
#else
   snprintf(buf, sizeof(buf), "%lf", val);
#endif
   config_set_string(conf, key, buf);
}

void config_set_float(config_file_t *conf, const char *key, float val)
{
   char buf[128];
   snprintf(buf, sizeof(buf), "%f", val);
   config_set_string(conf, key, buf);
}

void config_set_int(config_file_t *conf, const char *key, int val)
{
   char buf[128];
   snprintf(buf, sizeof(buf), "%d", val);
   config_set_string(conf, key, buf);
}

void config_set_hex(config_file_t *conf, const char *key, unsigned val)
{
   char buf[128];
   snprintf(buf, sizeof(buf), "%x", val);
   config_set_string(conf, key, buf);
}

void config_set_uint64(config_file_t *conf, const char *key, uint64_t val)
{
   char buf[128];
#ifdef _WIN32
   snprintf(buf, sizeof(buf), "%I64u", val);
#else
   snprintf(buf, sizeof(buf), "%llu", (long long unsigned)val);
#endif
   config_set_string(conf, key, buf);
}

void config_set_char(config_file_t *conf, const char *key, char val)
{
   char buf[2];
   snprintf(buf, sizeof(buf), "%c", val);
   config_set_string(conf, key, buf);
}

void config_set_bool(config_file_t *conf, const char *key, bool val)
{
   config_set_string(conf, key, val ? "true" : "false");
}

bool config_file_write(config_file_t *conf, const char *path)
{
   FILE *file;

   if (path)
   {
      file = fopen(path, "w");
      if (!file)
         return false;
   }
   else
      file = stdout;

   config_file_dump(conf, file);

   if (path)
      fclose(file);

   return true;
}

void config_file_dump(config_file_t *conf, FILE *file)
{
   struct include_list *includes = conf->includes;
   while (includes)
   {
      fprintf(file, "#include \"%s\"\n", includes->path);
      includes = includes->next;
   }

   struct config_entry_list *list = conf->entries;
   while (list)
   {
      if (!list->readonly)
         fprintf(file, "%s = \"%s\"\n", list->key, list->value);
      list = list->next;
   }
}

void config_file_dump_all(config_file_t *conf, FILE *file)
{
   struct include_list *includes = conf->includes;
   while (includes)
   {
      fprintf(file, "#include \"%s\"\n", includes->path);
      includes = includes->next;
   }

   struct config_entry_list *list = conf->entries;
   while (list)
   {
      fprintf(file, "%s = \"%s\" %s\n", list->key, list->value, list->readonly ? "(included)" : "");
      list = list->next;
   }
}

bool config_entry_exists(config_file_t *conf, const char *entry)
{
   struct config_entry_list *list = conf->entries;

   while (list)
   {
      if (strcmp(entry, list->key) == 0)
         return true;
      list = list->next;
   }

   return false;
}

bool config_get_entry_list_head(config_file_t *conf, struct config_file_entry *entry)
{
   const struct config_entry_list *head = conf->entries;
   if (!head)
      return false;

   entry->key   = head->key;
   entry->value = head->value;
   entry->next  = head->next;
   return true;
}

bool config_get_entry_list_next(struct config_file_entry *entry)
{
   const struct config_entry_list *next = entry->next;
   if (!next)
      return false;

   entry->key   = next->key;
   entry->value = next->value;
   entry->next  = next->next;
   return true;
}

