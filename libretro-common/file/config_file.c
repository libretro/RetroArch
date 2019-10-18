/* Copyright  (C) 2010-2018 The RetroArch team
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
#ifdef ORBIS
#include <sys/fcntl.h>
#include <orbisFile.h>
#endif
#include <retro_miscellaneous.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <compat/fopen_utf8.h>
#include <compat/msvc.h>
#include <file/config_file.h>
#include <file/file_path.h>
#include <lists/string_list.h>
#include <string/stdstring.h>
#include <streams/file_stream.h>

#define MAX_INCLUDE_DEPTH 16

struct config_entry_list
{
   /* If we got this from an #include,
    * do not allow overwrite. */
   bool readonly;

   char *key;
   char *value;
   struct config_entry_list *next;
};

struct config_include_list
{
   char *path;
   struct config_include_list *next;
};

static config_file_t *config_file_new_internal(
      const char *path, unsigned depth, config_file_cb_t *cb);

static int config_sort_compare_func(struct config_entry_list *a,
      struct config_entry_list *b)
{
   return (a && b) ? strcasecmp(a->key, b->key) : 0;
}

/* https://stackoverflow.com/questions/7685/merge-sort-a-linked-list */
static struct config_entry_list* merge_sort_linked_list(
         struct config_entry_list *list, int (*compare)(
         struct config_entry_list *one,struct config_entry_list *two))
{
   struct config_entry_list
         *right  = list,
         *temp   = list,
         *last   = list,
         *result = 0,
         *next   = 0,
         *tail   = 0;

   /* Trivial case. */
   if (!list || !list->next)
      return list;

   /* Find halfway through the list (by running two pointers,
    * one at twice the speed of the other). */
   while (temp && temp->next)
   {
      last  = right;
      right = right->next;
      temp  = temp->next->next;
   }

   /* Break the list in two. (prev pointers are broken here,
    * but we fix later) */
   last->next = 0;

   /* Recurse on the two smaller lists: */
   list = merge_sort_linked_list(list, compare);
   right = merge_sort_linked_list(right, compare);

   /* Merge: */
   while (list || right)
   {
      /* Take from empty lists, or compare: */
      if (!right)
      {
         next = list;
         list = list->next;
      }
      else if (!list)
      {
         next = right;
         right = right->next;
      }
      else if (compare(list, right) < 0)
      {
         next = list;
         list = list->next;
      }
      else
      {
         next = right;
         right = right->next;
      }

      if (!result)
         result = next;
      else
         tail->next = next;

      tail = next;
   }

   return result;
}

static char *strip_comment(char *str)
{
   /* Remove everything after comment.
    * Keep #s inside string literals. */
   char *string_end  = str + strlen(str);
   bool cut_comment  = true;

   while (!string_is_empty(str))
   {
      char *comment  = NULL;
      char *literal  = strchr(str, '\"');
      if (!literal)
         literal     = string_end;
      comment        = (char*)strchr(str, '#');

      if (!comment)
         comment     = string_end;

      if (cut_comment && literal < comment)
      {
         cut_comment = false;
         str         = literal + 1;
      }
      else if (!cut_comment && literal)
      {
         cut_comment = true;
         str         = (literal < string_end) ? literal + 1 : string_end;
      }
      else
      {
         *comment    = '\0';
         str         = comment;
      }
   }

   return str;
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
      if (*line == '"')
         return NULL;
      tok = strtok_r(line, "\"", &save);
   }
   /* We don't have that. Read until next space. */
   else if (*line != '\0') /* Nothing */
      tok = strtok_r(line, " \n\t\f\r\v", &save);

   if (tok && *tok)
      return strdup(tok);
   return NULL;
}

/* Move semantics? */
static void add_child_list(config_file_t *parent, config_file_t *child)
{
   struct config_entry_list *list = child->entries;
   if (parent->entries)
   {
      struct config_entry_list *head = parent->entries;
      while (head->next)
         head = head->next;

      /* set list readonly */
      while (list)
      {
         list->readonly = true;
         list           = list->next;
      }
      head->next        = child->entries;
   }
   else
   {
      /* set list readonly */
      while (list)
      {
         list->readonly = true;
         list           = list->next;
      }
      parent->entries   = child->entries;
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

static void add_sub_conf(config_file_t *conf, char *path, config_file_cb_t *cb)
{
   char real_path[PATH_MAX_LENGTH];
   config_file_t         *sub_conf  = NULL;
   struct config_include_list *head = conf->includes;
   struct config_include_list *node = (struct config_include_list*)
      malloc(sizeof(*node));

   if (node)
   {
      node->next = NULL;
      /* Add include list */
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

   real_path[0] = '\0';

#ifdef _WIN32
   if (!string_is_empty(conf->path))
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
      if (!string_is_empty(conf->path))
         fill_pathname_resolve_relative(real_path, conf->path,
               path, sizeof(real_path));
#endif

   sub_conf = (config_file_t*)
      config_file_new_internal(real_path, conf->include_depth + 1, cb);
   if (!sub_conf)
      return;

   /* Pilfer internal list. */
   add_child_list(conf, sub_conf);
   config_file_free(sub_conf);
}

static bool parse_line(config_file_t *conf,
      struct config_entry_list *list, char *line, config_file_cb_t *cb)
{
   char *key       = NULL;
   char *key_tmp   = NULL;
   size_t cur_size = 8;
   size_t idx      = 0;
   char *comment   = strip_comment(line);

   /* Starting line with #include includes config files. */
   if (comment == line)
   {
      comment++;
      if (strstr(comment, "include ") == comment)
      {
         char *include_line = comment + STRLEN_CONST("include ");
         char *path         = extract_value(include_line, false);

         if (!path)
            return false;

         if (conf->include_depth >= MAX_INCLUDE_DEPTH)
            fprintf(stderr, "!!! #include depth exceeded for config. Might be a cycle.\n");
         else
            add_sub_conf(conf, path, cb);
         free(path);
      }
   }

   /* Skips to first character. */
   while (isspace((int)*line))
      line++;

   key             = (char*)malloc(9);

   while (isgraph((int)*line))
   {
      if (idx == cur_size)
      {
         cur_size *= 2;
         key_tmp   = (char*)realloc(key, cur_size + 1);

         if (!key_tmp)
         {
            free(key);
            return false;
         }

         key = key_tmp;
      }

      key[idx++] = *line++;
   }
   key[idx]      = '\0';
   list->key     = key;

   list->value   = extract_value(line, true);

   if (!list->value)
   {
      list->key = NULL;
      free(key);
      return false;
   }

   return true;
}

static config_file_t *config_file_new_internal(
      const char *path, unsigned depth, config_file_cb_t *cb)
{
   RFILE              *file = NULL;
   struct config_file *conf = config_file_new_alloc();

   if (!path || !*path)
      return conf;
   conf->path          = strdup(path);
   if (!conf->path)
      goto error;

   conf->include_depth = depth;
   file                = filestream_open(path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!file)
   {
      free(conf->path);
      goto error;
   }

   while (!filestream_eof(file))
   {
      char *line                     = NULL;
      struct config_entry_list *list = (struct config_entry_list*)
         malloc(sizeof(*list));

      if (!list)
      {
         config_file_free(conf);
         filestream_close(file);
         return NULL;
      }

      list->readonly  = false;
      list->key       = NULL;
      list->value     = NULL;
      list->next      = NULL;

      line            = filestream_getline(file);

      if (!line)
      {
         free(list);
         continue;
      }

      if (*line && parse_line(conf, list, line, cb))
      {
         if (conf->entries)
            conf->tail->next = list;
         else
            conf->entries    = list;

         conf->tail = list;

         if (cb != NULL && list->key != NULL && list->value != NULL)
            cb->config_file_new_entry_cb(list->key, list->value) ;
      }

      free(line);

      if (list != conf->tail)
         free(list);
   }

   filestream_close(file);

   return conf;

error:
   free(conf);

   return NULL;
}

void config_file_free(config_file_t *conf)
{
   struct config_include_list *inc_tmp = NULL;
   struct config_entry_list *tmp       = NULL;
   if (!conf)
      return;

   tmp = conf->entries;
   while (tmp)
   {
      struct config_entry_list *hold = NULL;
      if (tmp->key)
         free(tmp->key);
      if (tmp->value)
         free(tmp->value);

      tmp->value = NULL;
      tmp->key   = NULL;

      hold       = tmp;
      tmp        = tmp->next;

      if (hold)
         free(hold);
   }

   inc_tmp = (struct config_include_list*)conf->includes;
   while (inc_tmp)
   {
      struct config_include_list *hold = NULL;
      if (inc_tmp->path)
         free(inc_tmp->path);
      hold    = (struct config_include_list*)inc_tmp;
      inc_tmp = inc_tmp->next;
      if (hold)
         free(hold);
   }

   if (conf->path)
      free(conf->path);
   free(conf);
}

bool config_append_file(config_file_t *conf, const char *path)
{
   config_file_t *new_conf = config_file_new_from_path_to_string(path);
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

config_file_t *config_file_new_from_string(const char *from_string,
      const char *path)
{
   size_t i;
   struct string_list *lines = NULL;
   struct config_file *conf  = (struct config_file*)malloc(sizeof(*conf));
   if (!conf)
      return NULL;

   if (!from_string)
      return conf;

   conf->path                     = NULL;
   conf->entries                  = NULL;
   conf->tail                     = NULL;
   conf->last                     = NULL;
   conf->includes                 = NULL;
   conf->include_depth            = 0;
   conf->guaranteed_no_duplicates = false ;

   if (!string_is_empty(path))
      conf->path                  = strdup(path);

   lines                          = string_split(from_string, "\n");
   if (!lines)
      return conf;

   for (i = 0; i < lines->size; i++)
   {
      struct config_entry_list *list = (struct config_entry_list*)
         malloc(sizeof(*list));
      char                    *line  = lines->elems[i].data;

      if (!list)
      {
         string_list_free(lines);
         config_file_free(conf);
         return NULL;
      }

      list->readonly  = false;
      list->key       = NULL;
      list->value     = NULL;
      list->next      = NULL;

      if (line && conf)
      {
         if (*line && parse_line(conf, list, line, NULL))
         {
            if (conf->entries)
               conf->tail->next = list;
            else
               conf->entries    = list;

            conf->tail          = list;
         }
      }

      if (list != conf->tail)
         free(list);
   }

   string_list_free(lines);

   return conf;
}

config_file_t *config_file_new_from_path_to_string(const char *path)
{
   int64_t length                = 0;
   uint8_t *ret_buf              = NULL;
   config_file_t *conf           = NULL;

   if (path_is_valid(path))
   {
      if (filestream_read_file(path, (void**)&ret_buf, &length))
      {
         if (length >= 0)
            conf = config_file_new_from_string((const char*)ret_buf, path);
         if ((void*)ret_buf)
            free((void*)ret_buf);
      }
   }

   return conf;
}

config_file_t *config_file_new_with_callback(
      const char *path, config_file_cb_t *cb)
{
   return config_file_new_internal(path, 0, cb);
}

config_file_t *config_file_new(const char *path)
{
   return config_file_new_internal(path, 0, NULL);
}

config_file_t *config_file_new_alloc(void)
{
   struct config_file *conf = (struct config_file*)malloc(sizeof(*conf));
   if (!conf)
      return NULL;

   conf->path                     = NULL;
   conf->entries                  = NULL;
   conf->tail                     = NULL;
   conf->last                     = NULL;
   conf->includes                 = NULL;
   conf->include_depth            = 0;
   conf->guaranteed_no_duplicates = false ;

   return conf;
}

static struct config_entry_list *config_get_entry(
      const config_file_t *conf,
      const char *key, struct config_entry_list **prev)
{
   struct config_entry_list *entry    = NULL;
   struct config_entry_list *previous = prev ? *prev : NULL;

   for (entry = conf->entries; entry; entry = entry->next)
   {
      if (string_is_equal(key, entry->key))
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

   if (!entry)
      return false;

   *in = strtod(entry->value, NULL);
   return true;
}

bool config_get_float(config_file_t *conf, const char *key, float *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key, NULL);

   if (!entry)
      return false;

   /* strtof() is C99/POSIX. Just use the more portable kind. */
   *in = (float)strtod(entry->value, NULL);
   return true;
}

bool config_get_int(config_file_t *conf, const char *key, int *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key, NULL);
   errno = 0;

   if (entry)
   {
      int val = (int)strtol(entry->value, NULL, 0);

      if (errno == 0)
      {
         *in = val;
         return true;
      }
   }

   return false;
}

bool config_get_size_t(config_file_t *conf, const char *key, size_t *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key, NULL);
   errno = 0;

   if (entry)
   {
      size_t val = 0;
      if (sscanf(entry->value, "%" PRI_SIZET, &val) == 1)
      {
         *in = val;
         return true;
      }
   }

   return false;
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
      {
         *in = val;
         return true;
      }
   }
   return false;
}
#endif

bool config_get_uint(config_file_t *conf, const char *key, unsigned *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key, NULL);
   errno = 0;

   if (entry)
   {
      unsigned val = (unsigned)strtoul(entry->value, NULL, 0);

      if (errno == 0)
      {
         *in = val;
         return true;
      }
   }

   return false;
}

bool config_get_hex(config_file_t *conf, const char *key, unsigned *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key, NULL);
   errno = 0;

   if (entry)
   {
      unsigned val = (unsigned)strtoul(entry->value, NULL, 16);

      if (errno == 0)
      {
         *in = val;
         return true;
      }
   }

   return false;
}

bool config_get_char(config_file_t *conf, const char *key, char *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key, NULL);

   if (entry)
   {
      if (entry->value[0] && entry->value[1])
         return false;

      *in = *entry->value;
      return true;
   }

   return false;
}

bool config_get_string(config_file_t *conf, const char *key, char **str)
{
   const struct config_entry_list *entry = config_get_entry(conf, key, NULL);

   if (!entry)
      return false;

   *str = strdup(entry->value);
   return true;
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
   return false;
}

bool config_get_path(config_file_t *conf, const char *key,
      char *buf, size_t size)
{
#if defined(RARCH_CONSOLE) || !defined(RARCH_INTERNAL)
   if (config_get_array(conf, key, buf, size))
      return true;
#else
   const struct config_entry_list *entry = config_get_entry(conf, key, NULL);

   if (entry)
   {
      fill_pathname_expand_special(buf, entry->value, size);
      return true;
   }
#endif
   return false;
}

bool config_get_bool(config_file_t *conf, const char *key, bool *in)
{
   const struct config_entry_list *entry = config_get_entry(conf, key, NULL);

   if (entry)
   {
      if (string_is_equal(entry->value, "true"))
         *in = true;
      else if (string_is_equal(entry->value, "1"))
         *in = true;
      else if (string_is_equal(entry->value, "false"))
         *in = false;
      else if (string_is_equal(entry->value, "0"))
         *in = false;
      else
         return false;
   }

   return entry != NULL;
}

void config_set_string(config_file_t *conf, const char *key, const char *val)
{
   struct config_entry_list *last  = (conf->guaranteed_no_duplicates && conf->last) ? conf->last : conf->entries;
   struct config_entry_list *entry = conf->guaranteed_no_duplicates?NULL:config_get_entry(conf, key, &last);

   if (entry && !entry->readonly)
   {
      if (entry->value)
         free(entry->value);
      entry->value = strdup(val);
      return;
   }

   if (!val)
      return;

   entry = (struct config_entry_list*)malloc(sizeof(*entry));
   if (!entry)
      return;

   entry->readonly  = false;
   entry->key       = strdup(key);
   entry->value     = strdup(val);
   entry->next      = NULL;

   if (last)
      last->next    = entry;
   else
      conf->entries = entry;

   conf->last       = entry;
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
#if defined(RARCH_CONSOLE) || !defined(RARCH_INTERNAL)
   config_set_string(conf, entry, val);
#else
   char buf[PATH_MAX_LENGTH];

   buf[0] = '\0';
   fill_pathname_abbreviate_special(buf, val, sizeof(buf));
   config_set_string(conf, entry, buf);
#endif
}

void config_set_double(config_file_t *conf, const char *key, double val)
{
   char buf[128];

   buf[0] = '\0';
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
   char buf[128];

   buf[0] = '\0';
   snprintf(buf, sizeof(buf), "%f", val);
   config_set_string(conf, key, buf);
}

void config_set_int(config_file_t *conf, const char *key, int val)
{
   char buf[128];

   buf[0] = '\0';
   snprintf(buf, sizeof(buf), "%d", val);
   config_set_string(conf, key, buf);
}

void config_set_uint(config_file_t *conf, const char *key, unsigned int val)
{
   char buf[128];

   buf[0] = '\0';
   snprintf(buf, sizeof(buf), "%u", val);
   config_set_string(conf, key, buf);
}

void config_set_hex(config_file_t *conf, const char *key, unsigned val)
{
   char buf[128];

   buf[0] = '\0';
   snprintf(buf, sizeof(buf), "%x", val);
   config_set_string(conf, key, buf);
}

void config_set_uint64(config_file_t *conf, const char *key, uint64_t val)
{
   char buf[128];

   buf[0] = '\0';
   snprintf(buf, sizeof(buf), "%" PRIu64, val);
   config_set_string(conf, key, buf);
}

void config_set_char(config_file_t *conf, const char *key, char val)
{
   char buf[2];

   buf[0] = '\0';
   snprintf(buf, sizeof(buf), "%c", val);
   config_set_string(conf, key, buf);
}

void config_set_bool(config_file_t *conf, const char *key, bool val)
{
   config_set_string(conf, key, val ? "true" : "false");
}

bool config_file_write(config_file_t *conf, const char *path, bool sort)
{
   if (!string_is_empty(path))
   {
#ifdef ORBIS
      int fd     = orbisOpen(path,O_RDWR|O_CREAT,0644);
      if (fd < 0)
         return false;
      config_file_dump_orbis(conf,fd);
      orbisClose(fd);
#else
      void* buf  = NULL;
      FILE *file = (FILE*)fopen_utf8(path, "wb");
      if (!file)
         return false;

      /* TODO: this is only useful for a few platforms, find which and add ifdef */
#if !defined(PS2) && !defined(PSP)
      buf = calloc(1, 0x4000);
      setvbuf(file, (char*)buf, _IOFBF, 0x4000);
#endif

      config_file_dump(conf, file, sort);

      if (file != stdout)
         fclose(file);
      if (buf)
         free(buf);
#endif
   }
   else
      config_file_dump(conf, stdout, sort);

   return true;
}

#ifdef ORBIS
void config_file_dump_orbis(config_file_t *conf, int fd)
{
   struct config_entry_list       *list = NULL;
   struct config_include_list *includes = conf->includes;
   while (includes)
   {
      char cad[256];
      sprintf(cad,"#include %s\n", includes->path);
      orbisWrite(fd, cad, strlen(cad));
      includes = includes->next;
   }

   list = merge_sort_linked_list((struct config_entry_list*)conf->entries, config_sort_compare_func);
   conf->entries = list;

   while (list)
   {
      if (!list->readonly && list->key)
      {
         char newlist[256];
         sprintf(newlist,"%s = %s\n", list->key, list->value);
         orbisWrite(fd, newlist, strlen(newlist));
      }
      list = list->next;
   }
}
#endif

void config_file_dump(config_file_t *conf, FILE *file, bool sort)
{
   struct config_entry_list       *list = NULL;
   struct config_include_list *includes = conf->includes;

   while (includes)
   {
      fprintf(file, "#include \"%s\"\n", includes->path);
      includes = includes->next;
   }

   if (sort)
      list = merge_sort_linked_list((struct config_entry_list*)
            conf->entries, config_sort_compare_func);
   else
      list = (struct config_entry_list*)conf->entries;

   conf->entries = list;

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
      if (string_is_equal(entry, list->key))
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

bool config_file_exists(const char *path)
{
   config_file_t *config = config_file_new(path);
   if (!config)
      return false;

   config_file_free(config);
   return true;
}

#if 0
static void test_config_file_parse_contains(
      const char * cfgtext,
      const char *key, const char *val)
{
   config_file_t *cfg = config_file_new_from_string(cfgtext, NULL);
   char          *out = NULL;
   bool            ok = false;

   if (!cfg)
      abort();

   ok = config_get_string(cfg, key, &out);
   if (ok != (bool)val)
      abort();
   if (!val)
      return;

   if (out == NULL)
      out = strdup("");
   if (strcmp(out, val) != 0)
      abort();
   free(out);
}

static void test_config_file(void)
{
   test_config_file_parse_contains("foo = \"bar\"\n",   "foo", "bar");
   test_config_file_parse_contains("foo = \"bar\"",     "foo", "bar");
   test_config_file_parse_contains("foo = \"bar\"\r\n", "foo", "bar");
   test_config_file_parse_contains("foo = \"bar\"",     "foo", "bar");

#if 0
   /* turns out it treats empty as nonexistent -
    * should probably be fixed */
   test_config_file_parse_contains("foo = \"\"\n",   "foo", "");
   test_config_file_parse_contains("foo = \"\"",     "foo", "");
   test_config_file_parse_contains("foo = \"\"\r\n", "foo", "");
   test_config_file_parse_contains("foo = \"\"",     "foo", "");
#endif

   test_config_file_parse_contains("foo = \"\"\n",   "bar", NULL);
   test_config_file_parse_contains("foo = \"\"",     "bar", NULL);
   test_config_file_parse_contains("foo = \"\"\r\n", "bar", NULL);
   test_config_file_parse_contains("foo = \"\"",     "bar", NULL);
}

/* compile with:
 gcc config_file.c -g -I ../include/ \
 ../streams/file_stream.c ../vfs/vfs_implementation.c ../lists/string_list.c \
 ../compat/compat_strl.c file_path.c ../compat/compat_strcasestr.c \
 && ./a.out
*/

int main(void)
{
	test_config_file();
}
#endif
