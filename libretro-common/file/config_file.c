/* Copyright  (C) 2010-2015 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (config_file.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#if !defined(_WIN32) && !defined(__CELLOS_LV2__) && !defined(_XBOX)
#include <sys/param.h> /* PATH_MAX */
#elif defined(_WIN32) && !defined(_XBOX)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(_XBOX)
#include <xtl.h>
#endif

#include <retro_miscellaneous.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <compat/msvc.h>
#include <file/config_file.h>
#include <file/file_path.h>
#include <retro_stat.h>
#include <string/string_list.h>
#include <rhash.h>

#define MAX_INCLUDE_DEPTH 16

struct config_entry_list
{
   /* If we got this from an #include,
    * do not allow overwrite. */
   bool readonly;
   char *key;
   char *value;
   uint32_t key_hash;

   struct config_entry_list *next;
};

struct config_include_list
{
   char *path;
   struct config_include_list *next;
};

struct config_file
{
   char *path;
   struct config_entry_list *entries;
   struct config_entry_list *tail;
   unsigned include_depth;

   struct config_include_list *includes;
};

static config_file_t *config_file_new_internal(const char *path, unsigned depth);
void config_file_free(config_file_t *conf);

static char *getaline(FILE *file)
{
   char* newline = (char*)malloc(9);
   char* newline_tmp = NULL;
   size_t cur_size   = 8;
   size_t idx        = 0;
   int in            = getc(file);

   if (!newline)
      return NULL;

   while (in != EOF && in != '\n')
   {
      if (idx == cur_size)
      {
         cur_size *= 2;
         newline_tmp = (char*)realloc(newline, cur_size + 1);

         if (!newline_tmp)
         {
            free(newline);
            return NULL;
         }

         newline = newline_tmp;
      }

      newline[idx++] = in;
      in = getc(file);
   }
   newline[idx] = '\0';
   return newline; 
}

static char *extract_value(char *line, bool is_value)
{
   char *save = NULL;
   char *tok  = NULL;

   if (is_value)
   {
      while (isspace((int)*line))
         line++;

      /* If we don't have an equal sign here,
       * we've got an invalid string. */
      if (*line != '=')
         return NULL;

      line++;
   }

   while (isspace((int)*line))
      line++;

   /* We have a full string. Read until next ". */
   if (*line == '"')
   {
      line++;
      tok = strtok_r(line, "\"", &save);
      if (!tok)
         return NULL;
      return strdup(tok);
   }
   else if (*line == '\0') /* Nothing */
      return NULL;

   /* We don't have that. Read until next space. */
   tok = strtok_r(line, " \n\t\f\r\v", &save);
   if (tok)
      return strdup(tok);
   return NULL;
}

static void set_list_readonly(struct config_entry_list *list)
{
   while (list)
   {
      list->readonly = true;
      list = list->next;
   }
}

/* Move semantics? */
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

   /* Rebase tail. */
   if (parent->entries)
   {
      struct config_entry_list *head = 
         (struct config_entry_list*)parent->entries;

      while (head->next)
         head = head->next;
      parent->tail = head;
   }
   else
      parent->tail = NULL;
}

static void add_include_list(config_file_t *conf, const char *path)
{
   struct config_include_list *head = conf->includes;
   struct config_include_list *node = (struct config_include_list*)calloc(1, sizeof(*node));

   if (!node)
      return;

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
   char real_path[PATH_MAX_LENGTH] = {0};
   config_file_t         *sub_conf = NULL;
   char                      *path = extract_value(line, false);

   if (!path)
      return;

   add_include_list(conf, path);

#ifdef _WIN32
   fill_pathname_resolve_relative(real_path, conf->path,
         path, sizeof(real_path));
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
      fill_pathname_resolve_relative(real_path, conf->path,
            path, sizeof(real_path));
#endif

   sub_conf = (config_file_t*)
      config_file_new_internal(real_path, conf->include_depth + 1);
   if (!sub_conf)
   {
      free(path);
      return;
   }

   /* Pilfer internal list. */
   add_child_list(conf, sub_conf);
   config_file_free(sub_conf);
   free(path);
}

static char *strip_comment(char *str)
{
   /* Remove everything after comment.
    * Keep #s inside string literals. */
   char *strend = str + strlen(str);
   bool cut_comment = true;

   while (*str)
   {
      char *comment = NULL;
      char *literal = strchr(str, '\"');
      if (!literal)
         literal = strend;
      comment = (char*)strchr(str, '#');
      if (!comment)
         comment = strend;

      if (cut_comment && literal < comment)
      {
         cut_comment = false;
         str = literal + 1;
      }
      else if (!cut_comment && literal)
      {
         cut_comment = true;
         str = literal + 1;
      }
      else if (comment)
      {
         *comment = '\0';
         str = comment;
      }
      else
         str = strend;
   }

   return str;
}

static bool parse_line(config_file_t *conf,
      struct config_entry_list *list, char *line)
{
   char *comment   = NULL;
   char *key       = (char*)malloc(9);
   char *key_tmp   = NULL;
   size_t cur_size = 8;
   size_t idx      = 0;

   if (!key)
      return false;

   if (!line || !*line)
   {
      free(key);
      return false;
   }

   comment = strip_comment(line);

   /* Starting line with # and include includes config files. */
   if ((comment == line) && (conf->include_depth < MAX_INCLUDE_DEPTH))
   {
      comment++;
      if (strstr(comment, "include ") == comment)
      {
         add_sub_conf(conf, comment + strlen("include "));
         free(key);
         return false;
      }
   }
   else if (conf->include_depth >= MAX_INCLUDE_DEPTH)
   {
      fprintf(stderr, "!!! #include depth exceeded for config. Might be a cycle.\n");
   }

   /* Skips to first character. */
   while (isspace((int)*line))
      line++;

   while (isgraph((int)*line))
   {
      if (idx == cur_size)
      {
         cur_size *= 2;
         key_tmp = (char*)realloc(key, cur_size + 1);

         if (!key_tmp)
         {
            free(key);
            return false;
         }

         key = key_tmp;
      }

      key[idx++] = *line++;
   }
   key[idx] = '\0';
   list->key = key;
   list->key_hash = djb2_calculate(key);

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
      conf->entries        = new_conf->entries; /* Pilfer. */
      new_conf->entries    = NULL;
   }

   config_file_free(new_conf);
   return true;
}

static config_file_t *config_file_new_internal(
      const char *path, unsigned depth)
{
   FILE *file = NULL;
   struct config_file *conf = (struct config_file*)calloc(1, sizeof(*conf));
   if (!conf)
      return NULL;

   if (!path || !*path)
      return conf;

   if (path_is_directory(path))
      goto error;

   conf->path = strdup(path);
   if (!conf->path)
      goto error;

   conf->include_depth = depth;
   file = fopen(path, "r");

   if (!file)
   {
      free(conf->path);
      goto error;
   }

   while (!feof(file))
   {
      struct config_entry_list *list = (struct config_entry_list*)
         calloc(1, sizeof(*list));
      char *line = NULL;

      if (!list)
      {
         config_file_free(conf);
         fclose(file);
         return NULL;
      }

      line = getaline(file);

      if (line)
      {
         if (parse_line(conf, list, line))
         {
            if (conf->entries)
               conf->tail->next = list;
            else
               conf->entries = list;

            conf->tail = list;
         }

         free(line);
      }
      else
      {
         free(list);
         continue;
      }

      if (list != conf->tail)
         free(list);
   }

   fclose(file);

   return conf;

error:
   free(conf);

   return NULL;
}

config_file_t *config_file_new_from_string(const char *from_string)
{
   size_t i;
   struct string_list *lines = NULL;
   struct config_file *conf = (struct config_file*)calloc(1, sizeof(*conf));
   if (!conf)
      return NULL;

   if (!from_string)
      return conf;

   conf->path = NULL;
   conf->include_depth = 0;
   
   lines = string_split(from_string, "\n");
   if (!lines)
      return conf;

   for (i = 0; i < lines->size; i++)
   {
      struct config_entry_list *list = (struct config_entry_list*)
         calloc(1, sizeof(*list));
      char* line = lines->elems[i].data;

      if (!list)
      {
         string_list_free(lines);
         config_file_free(conf);
         return NULL;
      }

      if (line)
      {
         if (parse_line(conf, list, line))
         {
            if (conf->entries)
               conf->tail->next = list;
            else
               conf->entries = list;

            conf->tail = list;
         }
      }

      if (list != conf->tail)
         free(list);
   }

   string_list_free(lines);

   return conf;
}

config_file_t *config_file_new(const char *path)
{
   return config_file_new_internal(path, 0);
}

void config_file_free(config_file_t *conf)
{
   struct config_include_list *inc_tmp = NULL;
   struct config_entry_list *tmp = NULL;
   if (!conf)
      return;

   tmp = conf->entries;
   while (tmp)
   {
      struct config_entry_list *hold = NULL;
      free(tmp->key);
      free(tmp->value);
      hold = tmp;
      tmp = tmp->next;
      free(hold);
   }

   inc_tmp = (struct config_include_list*)conf->includes;
   while (inc_tmp)
   {
      struct config_include_list *hold = NULL;
      free(inc_tmp->path);
      hold = (struct config_include_list*)inc_tmp;
      inc_tmp = inc_tmp->next;
      free(hold);
   }

   free(conf->path);
   free(conf);
}

static struct config_entry_list *config_get_entry(const config_file_t *conf,
      const char *key, struct config_entry_list **prev)
{
   struct config_entry_list *entry;
   struct config_entry_list *previous = NULL;

   uint32_t hash = djb2_calculate(key);

   if (prev)
      previous = *prev;

   for (entry = conf->entries; entry; entry = entry->next)
   {
      if (hash == entry->key_hash && !strcmp(key, entry->key))
         return entry;

      previous = entry;
   }

   if (prev)
      *prev = previous;

   return NULL;
}

bool config_get_double(config_file_t *conf, const char *key, double *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key, NULL);

   if (entry)
      *in = strtod(entry->value, NULL);

   return entry != NULL;
}

bool config_get_float(config_file_t *conf, const char *key, float *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key, NULL);

   if (entry)
   {
      /* strtof() is C99/POSIX. Just use the more portable kind. */
      *in = (float)strtod(entry->value, NULL);
   }

   return entry != NULL;
}

bool config_get_int(config_file_t *conf, const char *key, int *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key, NULL);
   errno = 0;

   if (entry)
   {
      int val = strtol(entry->value, NULL, 0);

      if (errno == 0)
         *in = val;
   }

   return entry != NULL && errno == 0;
}

#if defined(__STDC_VERSION__) && __STDC_VERSION__>=199901L
bool config_get_uint64(config_file_t *conf, const char *key, uint64_t *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key, NULL);
   errno = 0;

   if (entry)
   {
      uint64_t val = strtoull(entry->value, NULL, 0);

      if (errno == 0)
         *in = val;
   }

   return entry != NULL && errno == 0;
}
#endif

bool config_get_uint(config_file_t *conf, const char *key, unsigned *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key, NULL);
   errno = 0;

   if (entry)
   {
      unsigned val = strtoul(entry->value, NULL, 0);

      if (errno == 0)
         *in = val;
   }

   return entry != NULL && errno == 0;
}

bool config_get_hex(config_file_t *conf, const char *key, unsigned *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key, NULL);
   errno = 0;

   if (entry)
   {
      unsigned val = strtoul(entry->value, NULL, 16);

      if (errno == 0)
         *in = val;
   }

   return entry != NULL && errno == 0;
}

bool config_get_char(config_file_t *conf, const char *key, char *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key, NULL);

   if (entry)
   {
      if (entry->value[0] && entry->value[1])
         return false;

      *in = *entry->value;
   }

   return entry != NULL;
}

bool config_get_string(config_file_t *conf, const char *key, char **str)
{
   const struct config_entry_list *entry = config_get_entry(conf, key, NULL);

   if (entry)
      *str = strdup(entry->value);

   return entry != NULL;
}

bool config_get_config_path(config_file_t *conf, char *s, size_t len)
{
   if (!conf)
      return false;

   return strlcpy(s, conf->path, len);
}

bool config_get_array(config_file_t *conf, const char *key,
      char *buf, size_t size)
{
   const struct config_entry_list *entry = config_get_entry(conf, key, NULL);

   if (entry)
      return strlcpy(buf, entry->value, size) < size;

   return entry != NULL;
}

bool config_get_path(config_file_t *conf, const char *key,
      char *buf, size_t size)
{
#if defined(RARCH_CONSOLE)
   return config_get_array(conf, key, buf, size);
#else
   const struct config_entry_list *entry = config_get_entry(conf, key, NULL);

   if (entry)
      fill_pathname_expand_special(buf, entry->value, size);

   return entry != NULL;
#endif
}

bool config_get_bool(config_file_t *conf, const char *key, bool *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key, NULL);

   if (entry)
   {
      if (strcasecmp(entry->value, "true") == 0)
         *in = true;
      else if (strcasecmp(entry->value, "1") == 0)
         *in = true;
      else if (strcasecmp(entry->value, "false") == 0)
         *in = false;
      else if (strcasecmp(entry->value, "0") == 0)
         *in = false;
      else
         return false;
   }

   return entry != NULL;
}

void config_set_string(config_file_t *conf, const char *key, const char *val)
{
   struct config_entry_list *last  = conf->entries;
   struct config_entry_list *entry = config_get_entry(conf, key, &last);

   if (entry && !entry->readonly)
   {
      free(entry->value);
      entry->value = strdup(val);
      return;
   }

   entry = (struct config_entry_list*)calloc(1, sizeof(*entry));

   if (!entry)
      return;

   entry->key   = strdup(key);
   entry->value = strdup(val);

   if (last)
      last->next = entry;
   else
      conf->entries = entry;
}

void config_unset(config_file_t *conf, const char *key)
{
   struct config_entry_list *last  = conf->entries;
   struct config_entry_list *entry = config_get_entry(conf, key, &last);

   if (!entry)
      return;

   entry->key   = NULL;
   entry->value = NULL;
   free(entry->key);
   free(entry->value);
}

void config_set_path(config_file_t *conf, const char *entry, const char *val)
{
#if defined(RARCH_CONSOLE)
   config_set_string(conf, entry, val);
#else
   char buf[PATH_MAX_LENGTH] = {0};
   fill_pathname_abbreviate_special(buf, val, sizeof(buf));
   config_set_string(conf, entry, buf);
#endif
}

void config_set_double(config_file_t *conf, const char *key, double val)
{
   char buf[128] = {0};
#ifdef __cplusplus
   snprintf(buf, sizeof(buf), "%f", (float)val);
#elif defined(__STDC_VERSION__) && __STDC_VERSION__>=199901L
   snprintf(buf, sizeof(buf), "%lf", val);
#else
   snprintf(buf, sizeof(buf), "%f", (float)val);
#endif
   config_set_string(conf, key, buf);
}

void config_set_float(config_file_t *conf, const char *key, float val)
{
   char buf[128] = {0};
   snprintf(buf, sizeof(buf), "%f", val);
   config_set_string(conf, key, buf);
}

void config_set_int(config_file_t *conf, const char *key, int val)
{
   char buf[128] = {0};
   snprintf(buf, sizeof(buf), "%d", val);
   config_set_string(conf, key, buf);
}

void config_set_hex(config_file_t *conf, const char *key, unsigned val)
{
   char buf[128] = {0};
   snprintf(buf, sizeof(buf), "%x", val);
   config_set_string(conf, key, buf);
}

void config_set_uint64(config_file_t *conf, const char *key, uint64_t val)
{
   char buf[128] = {0};
#ifdef _WIN32
   snprintf(buf, sizeof(buf), "%I64u", val);
#else
   snprintf(buf, sizeof(buf), "%llu", (long long unsigned)val);
#endif
   config_set_string(conf, key, buf);
}

void config_set_char(config_file_t *conf, const char *key, char val)
{
   char buf[2] = {0};
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
   struct config_entry_list *list = NULL;
   struct config_include_list *includes = conf->includes;
   while (includes)
   {
      fprintf(file, "#include \"%s\"\n", includes->path);
      includes = includes->next;
   }

   list = (struct config_entry_list*)conf->entries;
   while (list)
   {
      if (!list->readonly && list->key)
         fprintf(file, "%s = \"%s\"\n", list->key, list->value);
      list = list->next;
   }
}

bool config_entry_exists(config_file_t *conf, const char *entry)
{
   struct config_entry_list *list = conf->entries;

   while (list)
   {
      if (!strcmp(entry, list->key))
         return true;
      list = list->next;
   }

   return false;
}

bool config_get_entry_list_head(config_file_t *conf,
      struct config_file_entry *entry)
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

